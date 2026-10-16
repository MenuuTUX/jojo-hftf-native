# Animation and move-frame data format (PS1 JoJo HFTF, reference analysis)

Source: disassembly of the exe functions at 0x800268BC (start), 0x80026A04 (tick), 0x80026A60 (advance),
0x80026960, 0x80026AFC, the handler table at 0x8005A8D8 and the 21 handlers (0x80026BA0 to 0x80027388),
checked against recorded RAM from a live fight (`runs/phase0-002/fight_inputs.pkl`, 170 per-frame snapshots, 4 objects).
Tools: `tools-phase0/mips.py` (disassembler), `tools-phase0/animdump.py` (stream dumper, analysis only).
All names are placeholders chosen for this document, not recovered Capcom names.

Confidence labels:
- **READ**: decoded directly from the instructions.
- **LIVE**: READ and additionally confirmed against recorded RAM.
- **GUESS**: meaning inferred, not shown by the code.

## 1. What this is

Every on-screen thing (player, Stand, effect) is one generic object of 0x390 bytes at a fixed address. Its behavior over time is a
**bytecode program** stored as bytes in the character's `PLxx.BIN` module. A small interpreter in the exe (not in the overlay) runs it.
A program is a list of commands. The most common command is a **frame record**: show one cell for N ticks, copy per-frame attributes and
a 16-byte hit-data row into the object, and move the object. Other commands are jumps, calls and small script variables.
The gameplay logic in the overlays decides which program to start (`set_anim`) and reads the fields the frame records filled in.

Consequence for a rewrite: the interpreter is tiny (about 300 instructions). The move data itself is DATA, not code, and can stay as a
byte format read from the user's disc, with a relocation step (section 7).

## 2. Object fields used by the interpreter (all offsets from the object base)

| Offset | Size | Name (placeholder) | Meaning | Confidence |
|---|---|---|---|---|
| +0x10 | u16 | cell | sprite cell id for drawing, copied from the record | READ |
| +0x12 | u8 | row_flags | copied from hit row byte 0xF; bit 0 compared with facing | LIVE |
| +0x1A | u8 | type | 0 = player, 1 = companion (Stand) in the fight seen; the code also treats 2, 6, 7 like players and others differently | LIVE (0 and 1) |
| +0x22 | u16 | hold | 1 means this frame never expires (tick does nothing) | READ |
| +0x28 | s16 | timer | ticks left for the current frame, decremented once per tick | LIVE |
| +0x2A.. | u8[] | vars | script variables, command operands pick `obj[0x2A + index]`; +0x2A itself is the "condition" variable for JNZ/JZ | READ |
| +0x34 | ptr | return_pc | set by CALL, read by RET | READ |
| +0x3C | ptr | pc | address of the next command to run | LIVE |
| +0x40 | ptr | last_record | address of the last frame record that ran | LIVE |
| +0x44 | ptr | base_pc | start of the current program (set by `set_anim`) | READ |
| +0x48 | ptr | row_table | 16-byte-row hit table for types other than 0, 1, 2, 6, 7 | READ |
| +0x4D, +0x4E | u8 | attr_a, attr_b | copied from record bytes 4 and 5 | READ |
| +0x4F | u8 | record_count | frames run since the program started | READ |
| +0x100 | u16 | box_set | copied from record bytes 6 and 7 (types 0, 1, 2, 6, 7) | READ |
| +0x12C | ptr | descriptor | per-character table descriptor; `descriptor+4` points to the 16-byte hit rows; `descriptor+0x14` points to 12-byte body-box rows | LIVE (+4), READ (+0x14) |
| +0xB | u8 | facing | 0 or 1; flips the sign of dx and of pushback | READ |
| +0x6A / +0x6E | s16 | x_int / y_int | integer position, changed once per frame record by hit-row dx, dy | LIVE |

## 3. Dispatch rule (READ, LIVE)

For the byte `b` at `pc`:
1. if `b & 0x80` the command class is 0 (frame record);
2. else if `b & 0x40` the class is 2 (short frame record);
3. else the class is `b` (a plain command, values 1 to 20; larger values are not valid).
The handler is the word at `0x8005A8D8 + 4 * class`. A loop (`set_anim` and `advance`) calls handlers until one leaves `timer != 0`; the frame record
and its variants are the ones that set the timer, so a program runs through jumps and variable commands in zero time until it reaches a frame.

`set_anim(obj, ptr)` (0x800268BC): `timer = 0; record_count = 0; pc = base_pc = ptr; hold = 0;` then run the loop.
`tick(obj)` (0x80026A04): if `hold != 0` return -1; if `timer > 0` then `timer -= 1`; if `timer == 0` then run `advance(obj, pc)` and return 0; else return 1.
`advance(obj, new_pc)` (0x80026A60): `timer = 0; hold = 0; pc = new_pc;` then run the loop.
0x80026960 restarts from `base_pc`. 0x80026AFC re-runs the loop from `pc` unless `hold` is set.

## 4. Command table

Operands are bytes of the command at `pc` (written `P[n]`). "skip" means `pc += P[1]` after the command, with `+2` when `pc & 3 != 0` for the commands that read a 32-bit target.
Class 0 and 2 records have their own length rule (section 5). "var(n)" means `obj[0x2A + n]`.

| Class | Name | Layout and effect | Confidence |
|---|---|---|---|
| 0 | FRAME | see section 5 | LIVE |
| 1 | JUMP | `P[4..5]`=high half, `P[6..7]`=low half of an **absolute 32-bit target**; `timer=0; record_count=0; pc=target` | READ; seen in data (idle loop jumps back to the first frame) |
| 2 | FRAME_SHORT | like FRAME but only bytes 1 to 5 are used (timer, cell, attr_a, attr_b); no hit row, no motion | READ |
| 3 | JNZ | if var(0) != 0: align `pc` to 4, `pc = *(u32*)(pc+4)`; else skip | READ |
| 4 | JZ | same with the opposite condition | READ |
| 5 | CLR | `var(0) = 0`; skip | READ |
| 6 | SET | `var(P[2]) = P[3]`; skip | READ |
| 7 | ADD | `var(P[2]) += P[3]`; skip | READ |
| 8 | CMP | `var(0) = (var(P[2]) == P[3]) ? 0xFF : 0`; skip | READ |
| 9 | AND | `var(P[2]) &= P[3]`; skip | READ |
| 10 | SET_ATTR | if `P[3] == 0` then `var(P[2]) = attr_a`; skip | READ |
| 11 | CALL | `return_pc = pc + 8; pc = absolute target (P[4..7] as in JUMP)` | READ |
| 12 | RET | `pc = return_pc` | READ |
| 13 | PAD_AND | `var(P[2]) = P[3] & input_bits()` where `input_bits` is the exe function at 0x80025BF8 (reads the pad words); skip | READ; the pad reading is by static reading only |
| 14 | JTABLE | table at `pc + P[1]` (+2 if `pc & 3`), entry index `var(u16 at P[2])`, target = 32-bit word at `table + 4 * index` | READ |
| 15 | FRAME_WIDE | `timer = u16 at P[2]` (bit 15 = hold, low 15 bits = ticks); attr_a = P[4]; attr_b = P[5]; skip by `P[1]` | READ |
| 16 | SET_FROM_21B | `var(u16 at P[2]) = obj[0x21B]`; skip | READ |
| 17 | SKIP | skip only | READ |
| 18 | SET_FROM_1FB | `var(u16 at P[2]) = obj[0x1FB]`; skip | READ |
| 19 | FRAME_BOXES | `timer` byte at P[2..3] like FRAME; `cell = u16 at P[4]`; attr_a = P[6]; attr_b = P[7]; `obj[0x108] = P[8]`, `obj[0x109] = P[9]`; copies two 8-byte rows from `descriptor + 8 * P[8]` and `descriptor + 8 * P[9]` to `obj+0x11C` and `obj+0x124`; skip by `P[1]` | READ; used by a different object kind (GUESS) |
| 20 | JREL | `pc = pc + int16(P[6]) + 4`; `timer = 0; record_count = 0` | READ |

Jump, call and table targets are absolute RAM addresses inside the character module, so data copies that load at a different base
need their targets rebased (the `PLxxX` relocated copies differ from `PLxx` by exactly +0x15800 in pointer words, 20,110 of 21,173 differing pointer words).

## 5. FRAME record (class 0), layout, confidence LIVE

Observed in the fight: every frame record had byte 0 = 0x8A (flag 0x80, length 0x0A). The format is:

| Byte | Meaning | Where it goes | Confidence |
|---|---|---|---|
| 0 | `0x80` flag marks a frame record; `b & 0x2F` is the record length in bytes (the next command starts there). Bit 0x10 and bit 0x40 are not used by dispatch | next pc = `pc + (b & 0x2F)` | LIVE: pointer advance matched on 226 of 226 transitions |
| 1 | bit 7 = hold flag; bits 0 to 6 = ticks to show this frame | `hold`, `timer` | READ |
| 2..3 | cell id (u16) | `+0x10` | READ |
| 4 | attribute A | `+0x4D` | READ |
| 5 | attribute B | `+0x4E` | READ |
| 6..7 | box set id (u16), only for types 0, 1, 2, 6, 7 | `+0x100` | READ |
| 8..9 | index (s16) into the 16-byte hit-row table | picks row `index` | LIVE |

Side effects of FRAME (READ): `last_record = pc`; `record_count += 1`; `+0x141` is moved to `+0x142` before being overwritten.

## 6. The 16-byte hit row (selected by FRAME byte 8..9)

Row address: `table + 16 * index`, where `table` = `*(descriptor + 4)` for types 0, 1, 2, 6, 7 (`descriptor` = `obj[0x12C]`), or `obj[0x48]` for the rest.
Mapping to the object, validated on 228 of 228 sampled rows for every listed byte (LIVE):

| Row byte | Copied to | Notes / who reads it (static) | Confidence on meaning |
|---|---|---|---|
| 0 | +0x13B | read by function 0x800BBD38 (AC_MAIN) | meaning unknown (GUESS: hit kind or damage class) |
| 1 | +0x13C | read by 0x800B1698, 0x800ABB64 | unknown |
| 2 | +0x13D | read by 0x800AF5C8, 0x800C1D74 | unknown |
| 3 | +0x13E | read by 7 functions (0x800C4080, 0x800B8168, ...) | unknown |
| 4 | +0x13F | read by 6 functions | unknown |
| 5 | `+0xF8` (signed, sign flipped when facing differs from row_flags bit 0) | pushback or horizontal speed; `+0xF8` is used by 28 functions | READ for the transform; GUESS for the name |
| 8 | +0x141 (old +0x141 goes to +0x142) | read by 0x800B3F64 and 0x800BBB50 | unknown |
| 9 | not copied; if non-zero calls `0x80024328(obj, value)` | GUESS: spawn or sound or effect id | READ for the call |
| 0xA | +0x140 | read by 0x800B7730, 0x800B77A8, 0x800B79E0, 0x800B9444 | unknown |
| 0xB | +0x143 | read by the body-push function 0x800AB884 | unknown |
| 0xC | +0xBB | read by exe function 0x80023778 (GUESS: draw layer) | unknown |
| 0xD | signed dx added to `x_int` (subtracted when facing = 1), once per record | root motion | READ |
| 0xE | signed dy added to `y_int`, once per record | root motion | READ |
| 0xF | +0x12 | bit 0 = side compare for row byte 5 | READ |

Bytes 6, 7 of the row are not read by the record handler (not copied); other code may read them directly from the row (not checked).

**Other object types (READ, added after review).** The mapping above is the path for types 0, 1, 2, 6 and 7 (`0x80026BF0..0x80026C10` tests `type < 3`, `== 7`, `== 6`). For every other type the handler (branch at `0x80026C08` to `0x80026CC0`) uses the row at `obj[0x48] + 16 * index` and skips the box-set copy (record bytes 6 and 7), the copies of row bytes 0 to 4, 8, 0xA, 0xB and 0xC, and the byte-9 call. It still copies record bytes 1 to 5 (timer, cell, attr_a, attr_b) and from the row copies only byte 5 (to `+0xF8`, only when non-zero), byte 0xD (dx), byte 0xE (dy) and byte 0xF (to `+0x12`). Timer, hold, `last_record`, `record_count` and the pc advance are common to all types.

## 7. Body boxes, hit test and the HIT.BIN tables (READ; corrected in pass 6)

Two functions in AC_MAIN use the 12-byte body-box rows at `*(descriptor + 0x14)` (row = `box_set * 12`, `box_set` = obj+0x100):
- **Push test `0x800AB884`** (184 instr): the row's u16 at +0xA is an index into a list of **8-byte rectangles** in the character's `PLxx_HIT.BIN`. The two HIT.BIN images are
  found through a two-word table at **0x800CAFB0** (in AC_MAIN) holding 0x800F3800 (slot A) and 0x80109000 (slot B), indexed by the signed byte at obj+0x136 (0 or 1). This is the reader of the
  `PLxx_HIT.BIN` files (so they are body/push rectangle lists, 4 KB each). Rectangle words +0, +2, +4, +6 are added to the objects' `x_int`/`y_int` and compared; when both objects overlap, one X moves by +d and the other by -d.
  (An earlier version of this document gave the table address as 0x800DAFB0; that was a typo, the instruction is `lui 0x800d; addiu -20560` = 0x800CAFB0.) Meaning of each rectangle word: GUESS (offsets and sizes).
- **Hit test `0x800AA8E8`** (109 instr; called from 8 sites): has two gates that an earlier version of this document missed (READ, added after review). Gate 1 (`0x800AA934`): if byte 5 of the attacker's 48-byte attack row (row `obj[0x138]` of `*(descriptor + 0x18)`) equals 30, the owning fighter at `0x800CDD50 + 0x390 * obj[0x136]` must have `+0x5` == 1 (state 1), otherwise the test returns 0. Gate 2 (`0x800AA994`): if the side mask byte at `0x80066CA2` is non-zero, `mask & (obj[0x136] + 1)` must be non-zero, otherwise it returns 0. After the gates it refuses unless the attacker's `+0x139` (already-hit latch) and `+0x169` are zero; takes the attacker's body-box row field at +0 (s16) and tests it
  with the helper `0x800AAA9C` against **three** defender fields at +4, +6 and +8 of the defender's body-box row; returns non-zero if any overlap and writes a 3-bit mask (bit 0, 1, 2 = which of the three tested).
  GUESS: the three defender fields are high, middle and low hurt zones.
- **Hit resolution `0x800A9828`** (308 instr): selects the attacker's **48-byte attack row** at `*(descriptor + 0x18) + 48 * obj[0x138]` and copies fields into the defender: row byte 6 to +0x149, byte 7 to +0x14A
  (reduced by defender +0x14E in some cases, minimum 1), 0xC to +0x147, 0xE to +0x14B, 0x15 to +0x14D, 0x1B to +0x159; also writes attacker pointer to defender +0x108, attacker facing to +0x106, defender facing flips.
  Meaning of those bytes (damage, hit-stun, knockback...) is GUESS; the life subtraction itself happens in other functions (36 store sites to +0x16A in AC_MAIN, see PHASE0_REPORT.md).
- **Contact damage loop `0x800A9280`** (67 instr): for objects flagged at +0x205, walks the enemy list (table at 0x80073B40, linked at +0x64), uses the hit test, and subtracts a fixed 10 from +0x16A.

## 8. What this means for a rewrite (not yet verified by implementation)

- The interpreter plus the 21 handlers is a few hundred lines of C. The commands are fixed-size bytes, so the stream can be read byte by byte with explicit endianness.
- The move data (frame records, hit rows, box tables) is data in `PLxx.BIN` and can be read from the extracted disc at runtime or at build time. A build step converts absolute pointers to
  offsets within the module (the PLxxX copies prove they are plain absolute addresses).
- Per-character code in `PLxx.BIN` (about 42 KB of code per character) decides the transitions (calls to `set_anim`, reads of the hit-row copies). That is where translation effort remains.
- A good first test for Phase 2: run the interpreter on a program stream from the extracted `PL00.BIN` and compare the sequence of (cell, timer, dx, dy, hit index) against what the reference build
  does for the same object, frame by frame.
- Length check (CORRECTED in pass 6, `tools-phase0/animwalk.py`): the first census walked streams from every pointer in the module and counted 8 percent records longer than 10 bytes. That was noise (walks spilled into pointer tables).
  Starting only from pointer targets whose walk is fault-free, all 26 modules contain 36,326 FRAME records of length 10, 3,138 FRAME_SHORT records of length 6 and 46 other-length records out of about 39,500; the ones I inspected
  (PL09 run of nine 40-byte "records") are tables of function pointers. **Conclusion: frame records are 10 bytes and short records 6 bytes; the earlier open item about long records is closed (retracted).**
  The rule `byte0 & 0x2F` is still what the code does, but no genuine record uses the 0x20 bit.
