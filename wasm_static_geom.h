#ifndef WASM_STATIC_GEOM_H
#define WASM_STATIC_GEOM_H

#ifdef __EMSCRIPTEN__

void wasm_static_geom_init(void);
void wasm_static_geom_draw(void);
void wasm_static_geom_shutdown(void);

#endif

#endif
