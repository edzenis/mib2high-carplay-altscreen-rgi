#ifndef ALT_SCENE_GEOMETRY_H
#define ALT_SCENE_GEOMETRY_H
/* Firmware-independent maneuver geometry. No OpenGL, window, socket, K2161 or
 * Audi APIs. Inspired by Luka's path/road/route separation, roundabout and
 * transition geometry. Reimplemented for portable fixed-function triangles.
 * Designed for deterministic host tests and GLES1/GLES2/software backends. */
#include <stdint.h>
#define ALT_SCENE_MAX_JUNCTIONS 18
#define ALT_SCENE_MAX_VERTICES 8192
#define ALT_SCENE_LAYER_COUNT 4

enum alt_scene_icon {
 ALT_SCENE_NONE=0, ALT_SCENE_APPROACH=1, ALT_SCENE_TURN=2,
 ALT_SCENE_UTURN=3, ALT_SCENE_MERGE=4, ALT_SCENE_EXIT=5,
 ALT_SCENE_ROUNDABOUT=6, ALT_SCENE_ARRIVED=7,
 ALT_SCENE_LANE_CHANGE=8, ALT_SCENE_ROUNDABOUT_EXIT=9
};
enum alt_scene_layer { ALT_SCENE_SHOULDER=0, ALT_SCENE_ASPHALT=1,
 ALT_SCENE_ACTIVE=2, ALT_SCENE_ACCENT=3 };
typedef struct {
 int icon;
 int exit_angle_deg;  /* signed degrees; -999 means unknown */
 int direction;       /* -1 left, 0 straight, 1 right */
 int driving_side;    /* 0 right hand traffic, 1 left hand */
 int junction_count;
 int junction_angles[ALT_SCENE_MAX_JUNCTIONS];
} alt_scene_maneuver;
typedef struct { float x,y; } alt_scene_point;
typedef struct {
 float xy[ALT_SCENE_LAYER_COUNT][ALT_SCENE_MAX_VERTICES*2];
 unsigned count[ALT_SCENE_LAYER_COUNT]; /* GL_TRIANGLES vertex counts */
 unsigned overflow;
} alt_scene_mesh;
/* Pure geometry: 1=scene generated, 0=unknown/invalid/empty (draw nothing). */
int alt_scene_build(const alt_scene_maneuver *m,alt_scene_mesh *out);
#endif
