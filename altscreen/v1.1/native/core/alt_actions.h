#ifndef ALTSCREEN_CORE_ACTIONS_H
#define ALTSCREEN_CORE_ACTIONS_H
/* Pure intent messages: stock AirPlay dictionaries belong exclusively in the output connector. */
typedef enum {
    ALT_ACTION_NONE=0,
    ALT_ACTION_SHOW_UI=1,
    ALT_ACTION_UPDATE_VIEW=2,
    ALT_ACTION_FORCE_KEYFRAME=3,
    ALT_ACTION_MAP_ZOOM=4
} alt_core_action_kind;
typedef struct {
    alt_core_action_kind kind;
    int view;
    int zoom_direction;
} alt_core_action;
/* Firmware-neutral capability contracts. A connector advertises support for the generic action contract. */
enum {
    ALT_CAP_CARPLAY_UI = (1u<<0),
    ALT_CAP_VIEWAREA = (1u<<1),
    ALT_CAP_VIDEO_MAP = (1u<<2),
    ALT_CAP_ZOOM = (1u<<4),
    ALT_CAP_RGI_IN = (1u<<5),
    ALT_CAP_HUD_GUIDANCE = (1u<<6),
    ALT_CAP_CLUSTER_DISPLAY = (1u<<7),
    ALT_CAP_GAUGE_INPUT = (1u<<8)
};

__attribute__((visibility("hidden"))) alt_core_action alt_core_action_for_stage(int stage,int view);
__attribute__((visibility("hidden"))) alt_core_action alt_core_action_zoom(int direction);
__attribute__((visibility("hidden"))) int alt_core_can_dispatch(unsigned int capabilities,alt_core_action a);
#endif
