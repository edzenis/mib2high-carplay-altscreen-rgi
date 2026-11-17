#include "alt_actions.h"
#include "alt_core.h"
alt_core_action alt_core_action_for_stage(int stage,int view) {
    alt_core_action a={ALT_ACTION_NONE,view,0};
    switch(stage){
        case ALT_CORE_SHOW:a.kind=ALT_ACTION_SHOW_UI;break;
        case ALT_CORE_VIEW:a.kind=ALT_ACTION_UPDATE_VIEW;break;
        case ALT_CORE_KEYFRAME:a.kind=ALT_ACTION_FORCE_KEYFRAME;break;
        default:break;
    }
    return a;
}
alt_core_action alt_core_action_zoom(int direction) {
    alt_core_action a={ALT_ACTION_NONE,0,direction};
    if(direction==0 || direction==1)a.kind=ALT_ACTION_MAP_ZOOM;
    return a;
}

int alt_core_can_dispatch(unsigned int caps,alt_core_action a) {
    switch(a.kind) {
        case ALT_ACTION_SHOW_UI: return (caps&ALT_CAP_CARPLAY_UI) != 0;
        case ALT_ACTION_UPDATE_VIEW: return (caps&ALT_CAP_VIEWAREA) != 0;
        case ALT_ACTION_FORCE_KEYFRAME: return (caps&ALT_CAP_VIDEO_MAP) != 0;
        case ALT_ACTION_MAP_ZOOM: return (caps&ALT_CAP_ZOOM) != 0;
        default: return 0;
    }
}
