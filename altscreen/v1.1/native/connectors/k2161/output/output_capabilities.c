#include "core/alt_actions.h"
#include "connectors/k2161/composition/capabilities.h"
unsigned int k2161_output_capabilities(void){
 return ALT_CAP_CARPLAY_UI|ALT_CAP_VIEWAREA|ALT_CAP_VIDEO_MAP|ALT_CAP_ZOOM|ALT_CAP_HUD_GUIDANCE|ALT_CAP_CLUSTER_DISPLAY;
}
