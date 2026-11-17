#ifndef ALTSCREEN_VIDEO_CORE_H
#define ALTSCREEN_VIDEO_CORE_H
#include <stdint.h>
#include <stddef.h>
#define ALT_CORE_AV11_MAGIC 0x31315641U
#define ALT_CORE_AV11_HEADER_LEN 20U
/* Return byte count, or zero if malformed or too large. All fields LE. */
__attribute__((visibility("hidden"))) size_t alt_core_av11_pack(uint8_t *out,size_t cap,const uint8_t *data,size_t length,uint64_t pts,uint32_t flag);
#endif
