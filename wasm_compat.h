#ifndef WASM_COMPAT_H
#define WASM_COMPAT_H

#ifdef __EMSCRIPTEN__

/* Emscripten's LEGACY_GL_EMULATION provides single-precision fixed-function
   GL but not the double-precision variants nor a handful of GL/GLU/GLUT
   entry points the original native build relies on. This header makes the
   native sources link cleanly under emcc with no per-call-site #ifdefs. */

/* d → f scalar variants. Implicit double→float conversion at the call site
   is fine for graphics precision. */
#define glVertex3d    glVertex3f
#define glNormal3d    glNormal3f
#define glRotated     glRotatef
#define glScaled      glScalef
#define glTranslated  glTranslatef

/* glRasterPos3* (both d and f) are absent from the emulator. The only
   callers pair it with Print() for axis labels — and Print is a no-op
   here — so stubbing the positioning is harmless. */
#define glRasterPos3d(x, y, z) ((void)0)

/* glVertex3dv has no f-only equivalent that takes a double pointer; convert
   the 3 doubles into a stack float[3] and forward to glVertex3fv. */
static inline void wasm_glVertex3dv(const double v[3])
{
    float f[3] = { (float)v[0], (float)v[1], (float)v[2] };
    glVertex3fv(f);
}
#define glVertex3dv wasm_glVertex3dv

/* glWindowPos2i is not in the emulator. Callers use it solely to position
   debug text drawn by Print(), which is itself a no-op on wasm (see
   print.c), so the call can also be elided. */
#define glWindowPos2i(x, y) ((void)0)

/* glLightModeli(pname, int) — bridge to the fv form the emulator supports. */
static inline void wasm_glLightModeli(GLenum pname, GLint param)
{
    GLfloat p = (GLfloat)param;
    glLightModelfv(pname, &p);
}
#define glLightModeli wasm_glLightModeli

/* glColorMaterial is absent from the emulator. Dropping it means glColor*
   calls won't update material colors under lighting; visuals will be
   slightly different on the web build, but everything links and renders. */
#define glColorMaterial(face, mode) ((void)0)

/* glMaterialf(face, pname, GLfloat) — bridge to fv. */
static inline void wasm_glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    glMaterialfv(face, pname, &param);
}
#define glMaterialf wasm_glMaterialf

#endif /* __EMSCRIPTEN__ */

#endif /* WASM_COMPAT_H */
