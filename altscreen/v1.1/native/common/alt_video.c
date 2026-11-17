#include "alt_video.h"
#include <string.h>
static void le32(uint8_t *dst,uint32_t v) {
    dst[0]=(uint8_t)v;dst[1]=(uint8_t)(v>>8);dst[2]=(uint8_t)(v>>16);dst[3]=(uint8_t)(v>>24);
}
size_t alt_core_av11_pack(uint8_t *out,size_t cap,const uint8_t *data,size_t n,uint64_t pts,uint32_t flag) {
    if(!out || !data || !n || n>UINT32_MAX || cap<ALT_CORE_AV11_HEADER_LEN || n>cap-ALT_CORE_AV11_HEADER_LEN) return 0;
    le32(out,ALT_CORE_AV11_MAGIC);le32(out+4,(uint32_t)n);
    le32(out+8,(uint32_t)pts);le32(out+12,(uint32_t)(pts>>32));le32(out+16,flag);
    memcpy(out+ALT_CORE_AV11_HEADER_LEN,data,n);
    return ALT_CORE_AV11_HEADER_LEN+n;
}
