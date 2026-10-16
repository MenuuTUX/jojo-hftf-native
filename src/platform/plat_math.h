/* Native replacements for the title's software floating-point helpers and
 * the documented libc memory routines. Arguments are raw guest bit patterns;
 * memory routines receive host pointers. */
#ifndef PS1_PORT_PLATFORM_MATH_H
#define PS1_PORT_PLATFORM_MATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t plat_sf_add_f32(uint32_t a, uint32_t b);      /* 0x8003d020 */
uint32_t plat_sf_ne_f32(uint32_t a, uint32_t b);       /* 0x8003d1d0 */
int32_t  plat_sf_f64_to_i32(uint32_t lo, uint32_t hi); /* 0x8003d200 */
int32_t  plat_sf_f32_to_i32(uint32_t bits);            /* 0x8003d3c0 */
void     plat_sf_i32_to_f64(uint32_t bits, uint32_t *lo, uint32_t *hi); /* 0x8003d470 */
uint32_t plat_sf_i32_to_f32(uint32_t bits);             /* 0x8003d650 */
uint32_t plat_sf_mul_f32(uint32_t a, uint32_t b);       /* 0x8003d720 */
void     plat_sf_sub_f64(uint32_t alo, uint32_t ahi, uint32_t blo, uint32_t bhi,
                         uint32_t *lo, uint32_t *hi);    /* 0x8003d860 */
void     plat_sf_fragment_dc00(uint32_t v0, uint32_t *out_v0, uint32_t *out_v1);
uint32_t plat_sf_sub_f32(uint32_t a, uint32_t b);       /* 0x8003ddc0 */

void *plat_bcopy(const void *src, void *dst, size_t n); /* guest bcopy returns src */
void  plat_bzero(void *dst, size_t n);
void *plat_memset(void *dst, int value, size_t n);

#ifdef __cplusplus
}
#endif

#endif
