/*
 *  Print message to stderr and exit
 */
#include "CSCIx229.h"

__attribute__((noreturn)) void Fatal(const char* format , ...)
{
   va_list args;
   va_start(args,format);
   vfprintf(stderr,format,args);
   va_end(args);
   exit(1);
}

#ifdef __EMSCRIPTEN__
/* Single definition for the glColorMaterial emulation state declared
   extern in wasm_compat.h. See that header for rationale. */
GLenum g_wasm_cm_face = GL_FRONT_AND_BACK;
GLenum g_wasm_cm_mode = GL_AMBIENT_AND_DIFFUSE;
int    g_wasm_cm_on   = 0;
#endif
