#ifndef WASM_STATIC_GEOM_H
#define WASM_STATIC_GEOM_H

#ifdef __EMSCRIPTEN__

void wasm_static_geom_init(void);
void wasm_static_geom_draw(void);
void wasm_static_geom_shutdown(void);
void wasm_static_geom_set_light(const float pos[3], const float ambient[3], const float diffuse[3], int enabled);

#endif

#endif
