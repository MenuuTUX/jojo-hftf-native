/*
 * gte.h - PlayStation GTE (COP2 geometry engine) fixed-point math, plain C99.
 *
 * The register file is stored exactly as the guest sees it: 32 data words and
 * 32 control words. Accessors apply the hardware read/write semantics
 * (sign extension, packed-pair layout, IRGB/ORGB and LZCR derivation, FIFO
 * pushes). plat_gte_command runs one COP2 command word (bits 0..5 = opcode,
 * bit 10 = lm, bit 19 = sf, bits 13..18 = MVMVA selects).
 */
#ifndef PS1_PORT_PLATFORM_GTE_H
#define PS1_PORT_PLATFORM_GTE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plat_gte_state {
    uint32_t data[32];   /* COP2 data registers  r0..r31 (MFC2/MTC2 numbering) */
    uint32_t ctrl[32];   /* COP2 control registers r0..r31 (CFC2/CTC2 numbering) */
} plat_gte_state;

/* Start with all data and control registers clear. */
void plat_gte_init(plat_gte_state *s);

/* Register reads. Out-of-range reg (>= 32) returns 0. */
uint32_t plat_gte_read_data(const plat_gte_state *s, unsigned reg);
uint32_t plat_gte_read_ctrl(const plat_gte_state *s, unsigned reg);

/* Register writes. Out-of-range reg (>= 32) is ignored. */
void plat_gte_write_data(plat_gte_state *s, unsigned reg, uint32_t value);
void plat_gte_write_ctrl(plat_gte_state *s, unsigned reg, uint32_t value);

/* Run one GTE command word. Unknown opcodes leave the state unchanged. */
void plat_gte_command(plat_gte_state *s, uint32_t command);

#ifdef __cplusplus
}
#endif

#endif /* PS1_PORT_PLATFORM_GTE_H */
