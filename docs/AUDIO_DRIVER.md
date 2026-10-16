# Capcom audio code above libsnd and libds (SLUS_010.60)

Written 2026-10-12 from the IDA Hex-Rays output of the exe (all labels READ, from decompilation only, not yet confirmed by a live trace unless stated). Function names are addresses. The libsnd/libspu and libds layers below this are described in `ida/IDA_RESULTS.md` and `PHASE1_SURFACE.md`.

## Sound effects and voice play: a record interpreter

The game never uses the PSY-Q sequence player (no SEQ files exist, `SsSeqOpen` is never called). Capcom drives SPU voices directly.

- `0x80023AF4(id, a2, a3, a4, a5)` is the play-sound entry. `id == 128` stops everything: `SsUtAllKeyOff(0)` and clears the voice tables at `0x8008A000 + 19368..19380`. Otherwise `byte_8008A000[id]` selects a bank (stored in `word_80089FF8`), and `off_8005A7D0[bank]` is a table of record lists; the list for `a3` is walked 8 bytes per record until the record's second word has bit 23 clear.
- `0x80023C48(record, id, a3, a4, a5, a6)` interprets one 8-byte record. The record type is the high nibble of byte 1 of the first word:
  - **0 (key on).** Voice number is bits 7..11 of the second word, plus 10 when `a3 != 0` (second player). Voice priority is a byte table at `byte_8008A000 + 19292 + voice`: a key-on is accepted when its priority (bits 16..22 of the second word) is not below the stored one, or when `SpuGetKeyStatus(1 << voice)` shows the voice is free. Arguments of `SsUtKeyOnV(voice, vab, program, tone, note, fine, left, right)` come from the record: program is byte 3, tone is bits 12..15 of the second word, note is the low 7 bits, volumes are derived from bits 24..30 and the pan byte (record byte 2; 255 means "from the caller's pan `a5`", scaled by 1/3 of `a5 + 192`). Pan above 63 lowers the left volume, below lowers the right. The final volumes are scaled by `byte_8006CE59` (SFX volume option) unless `byte_8008EBB9` (a mute/flag) is set. A note offset from a small per-player table (`dword_8005A7DC`, `dword_8005A7E4`) is added when `a6 != -1` (pitch variation per slot).
  - **8 (indirect list).** Walks another record list, `(**off_8005A7D0[id])[2 * index]`, recursively, with a toggle flag per voice slot (`byte_8008A000 + 19332`) used for alternating sounds.
  - **9 (volume).** Index 4095 sets the master volume `SsSetMVol(v, v)`, otherwise `SsUtSetVVol(voice, v, v)`.
  - **11 (key off).** Index 4095 is `sub_8003F940(0)` (all key off), otherwise key-off of one voice when the record priority is not below the stored one, and the stored priority is cleared.
- `0x80023A60` resets the tables (all key off plus clearing the four slot tables and the 24 voice priorities).
- The 15 callers of `0x80023AF4` are the game's sound-trigger helpers at `0x800241C0..0x80024C7C` and `0x800294C4`; the overlay code calls these helpers, never libsnd directly.

For the native port this means: a sound effect is data (the record lists in the exe data and overlay data) plus about 100 lines of C. The libsnd layer underneath needs only key on, key off, volume set, master volume and the key status.

## Music: CD-XA through a track state machine

All music is CD-XA streamed by the libds layer; there is no SPU-resident music.

- The job struct is at `0x80079208`; the global mode bytes are `0x80079230` (loader state), `0x80079232` (Setmode byte for data reads, initial 0xA0), `0x80079233` (initial 0xC8).
- **Select a track** `0x80017448(job, id)`: copies the start sector and the per-track values from a table (`0x8005631C`, 3 words per track), computes the end sector from `0x800599E0` (ids below 9) or `0x80059B00` (others), stores the filter (file 1, channel = low byte of the id), pauses (`DsControl(9)`), and sets the CD volume (`SsSetSerialAttr(0,0,1)`, `SsSetSerialVol(0, v, v)`, then `0x800173E4(v)` which calls `DsMix`).
- **Start** `0x800176E8(job)`: waits for the queue to drain, then retries each of these until it returns nonzero: `DsControl(9 Pause)`, `DsControl(14 Setmode, job+43)`, `DsControl(13 Setfilter, job+4)`, `DsControl(21 SeekL, job+8)`, `DsControl(27 ReadS)`.
- **Poll** `0x80017648`: each frame calls the state handler from `off_800599AC[byte_80079235]`; when the command queue is empty it sets the sync callback `0x80017C14` and issues `DsControl(17 GetlocP)`; the callback converts the result to a sector number (`dword_8007921C`), compared with the track end for loop or stop. **Loop** `0x800179A8` seeks back to the start and issues ReadS again. **Stop** `0x80017884` pauses and handles fade state.
- **Fade** `0x80017B68`: steps the 16-bit fade value in the job by a rate each frame and calls `0x800173E4(v >> 8)`.

## Data loader (ReadN path)

`0x80017D20` is the per-frame loader state machine (byte `0x80079230`): case 1 is the music poll above, 3 and 5 are file loads (`0x80018FEC`, `0x800193A8`), case 4 is the data stream with an error recovery rule: when `DsStatus() & 0x20` and `DsSync(-1, buf) == 5` the ReadN is re-issued. Callbacks: ReadN `0x8001824C` (status 2 starts the ready system with alarm `0x800182D4`, any other status re-issues ReadN), SeekL `0x800181D8` (status 2 sets `byte_80079246`, otherwise re-issues), sync `0x80017CC8` (status 5 sets `byte_80079231`, otherwise copies the 8-byte result block and sets `byte_8007923A`).

## Consequences for the port

1. The libds replacement must implement `DsControl` commands 9, 13, 14, 17, 21, 27 and `DsControlB` commands 6 and 21 with the callbacks above, `DsStatus`, `DsSync(-1)`, `DsMix`, `DsReadyCallback`, `DsSyncCallback`, `DsStartReadySystem`.
2. The position returned by `GetlocP` must advance with the streamed sectors, because the end of a track is detected by comparing it with the stored end sector.
3. Open question: the boot movies play XA audio before the first libds ReadN (reference CD input is active in frames 0 to 311); the code path has not been identified yet.
