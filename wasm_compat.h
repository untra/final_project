#ifndef WASM_COMPAT_H
#define WASM_COMPAT_H

#ifdef __EMSCRIPTEN__

#include <string.h>

/* Emscripten's LEGACY_GL_EMULATION provides single-precision fixed-function
   GL but not the double-precision variants nor a handful of GL/GLU/GLUT
   entry points the original native build relies on. This header makes the
   native sources link cleanly under emcc with no per-call-site #ifdefs. */

/* d → f scalar variants. Implicit double→float conversion at the call site
   is fine for graphics precision. The glVertex3d / glNormal3d aliases chain
   into the wrappers defined further down (macro expansion is top-down at
   the point of use, so `glVertex3d(x,y,z)` → `glVertex3f(x,y,z)` →
   `wasm_vertex3f(x,y,z)` once that wrapper is in scope). */
#define glVertex3d    glVertex3f
#define glNormal3d    glNormal3f
#define glRotated     glRotatef
#define glScaled      glScalef
#define glTranslated  glTranslatef

/* glRasterPos3* (both d and f) are absent from the emulator. The only
   callers pair it with Print() for axis labels — and Print is a no-op
   here — so stubbing the positioning is harmless. */
#define glRasterPos3d(x, y, z) ((void)0)

/* glWindowPos2i is not in the emulator. Callers use it solely to position
   debug text drawn by Print(), which is itself a no-op on wasm (see
   print.c), so the call can also be elided. */
#define glWindowPos2i(x, y) ((void)0)

/* glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, *) is unimplemented in
   LEGACY_GL_EMULATION (aborts with "TODO: 2897"). Emscripten only handles
   GL_LIGHT_MODEL_AMBIENT. LOCAL_VIEWER is a specular-calculation hint;
   disabling it (the GL default) is a near-imperceptible visual change. */
static inline void wasm_glLightModelfv(GLenum pname, const GLfloat* params)
{
    if (pname == GL_LIGHT_MODEL_LOCAL_VIEWER) return;
    glLightModelfv(pname, params);
}
#define glLightModelfv wasm_glLightModelfv

/* glLightModeli(pname, int) — bridge to the fv form the emulator supports.
   After the #define above, glLightModelfv here resolves to the filtered
   wrapper, so an i-call with LOCAL_VIEWER also gets dropped. */
static inline void wasm_glLightModeli(GLenum pname, GLint param)
{
    GLfloat p = (GLfloat)param;
    glLightModelfv(pname, &p);
}
#define glLightModeli wasm_glLightModeli

/* glColorMaterial is missing from LEGACY_GL_EMULATION's glEnable/glDisable
   plumbing. The scene relies on it: `glColorMaterial(FRONT_AND_BACK,
   AMBIENT_AND_DIFFUSE) + glEnable(GL_COLOR_MATERIAL)` is set once in
   final.c, and from then on every glColor3f under glBegin/glEnd is
   expected to set the surface's ambient+diffuse material. Without that,
   surfaces render against the default material (ambient 0.2, diffuse 0.8)
   regardless of the per-vertex glColor — most of the bowling alley turns
   nearly black under the dim Ambient/Diffuse lights.

   We can't call the real glColorMaterial. Emulate it: track the most
   recent enabled mode/face and patch glColor* below to also call
   glMaterialfv. Default to FRONT_AND_BACK + AMBIENT_AND_DIFFUSE since
   that's the only combination this codebase uses; the storage is here so
   if glColorMaterial is called explicitly we honor whatever was set.

   These are `extern` because the flag is set in one TU (final.c, where
   glColorMaterial + glEnable(GL_COLOR_MATERIAL) are called once) and
   read in another (bowling.c, every glColor3f under glBegin/glEnd). The
   single definition lives in fatal.c, which is in WASM_SRCS. */
extern GLenum g_wasm_cm_face;
extern GLenum g_wasm_cm_mode;
extern int    g_wasm_cm_on;

static inline void wasm_glColorMaterial(GLenum face, GLenum mode)
{
    g_wasm_cm_face = face;
    g_wasm_cm_mode = mode;
}
#define glColorMaterial wasm_glColorMaterial

/* Intercept glEnable/glDisable so GL_COLOR_MATERIAL toggles our flag
   instead of being forwarded into Emscripten (which would abort or
   silently ignore it). Everything else passes through. */
static inline void wasm_glEnable(GLenum cap)
{
    if (cap == GL_COLOR_MATERIAL) { g_wasm_cm_on = 1; return; }
    glEnable(cap);
}
#define glEnable wasm_glEnable

static inline void wasm_glDisable(GLenum cap)
{
    if (cap == GL_COLOR_MATERIAL) { g_wasm_cm_on = 0; return; }
    glDisable(cap);
}
#define glDisable wasm_glDisable

/* glMaterialfv(*, GL_EMISSION, *) is unimplemented in LEGACY_GL_EMULATION
   (it aborts with "TODO: 5632"). Emission in this scene is decorative — the
   ball, lanes, and mural all have textures + glColor + diffuse lighting that
   already determine how they look. Dropping the call is a closer visual
   match than substituting into a supported pname would be. */
static inline void wasm_glMaterialfv(GLenum face, GLenum pname, const GLfloat* params)
{
    if (pname == GL_EMISSION) return;
    if (pname == GL_AMBIENT_AND_DIFFUSE) {
        /* Emscripten's glMaterialfv aborts on this combined pname (TODO: 5634).
           Split into the two individual pnames it does support. The glColor*
           shim below relies on this path for emulating GL_COLOR_MATERIAL. */
        glMaterialfv(face, GL_AMBIENT, params);
        glMaterialfv(face, GL_DIFFUSE, params);
        return;
    }
    glMaterialfv(face, pname, params);
}
#define glMaterialfv wasm_glMaterialfv

/* glMaterialf(face, pname, GLfloat) — bridge to fv. Now routes into the
   filtered wasm_glMaterialfv above. */
static inline void wasm_glMaterialf(GLenum face, GLenum pname, GLfloat param)
{
    glMaterialfv(face, pname, &param);
}
#define glMaterialf wasm_glMaterialf

/* gluLookAt under Emscripten's LEGACY_GL_EMULATION silently fails: the
   call returns without modifying GL_MODELVIEW_MATRIX, so every scene
   renders from an identity view regardless of camera input. Verified
   in round-2 diagnostics — four wildly different camera positions
   produced pixel-identical canvas pixels. Replace it with manual view
   matrix construction loaded directly via glLoadMatrixf.

   final.c calls gluLookAt only after glLoadIdentity, so a load (rather
   than a multiply) preserves call-site semantics. Macro override
   evaluates each argument exactly once and the only call-site
   expressions are simple variables / Cos(ph), so there are no
   side-effect hazards. */
#include <math.h>
static inline void wasm_glu_lookat(double ex, double ey, double ez,
                                   double cx, double cy, double cz,
                                   double ux, double uy, double uz)
{
    double fx = cx - ex, fy = cy - ey, fz = cz - ez;
    double fn = 1.0 / sqrt(fx*fx + fy*fy + fz*fz);
    fx *= fn; fy *= fn; fz *= fn;
    double sx = fy*uz - fz*uy;
    double sy = fz*ux - fx*uz;
    double sz = fx*uy - fy*ux;
    double sn = 1.0 / sqrt(sx*sx + sy*sy + sz*sz);
    sx *= sn; sy *= sn; sz *= sn;
    double u2x = sy*fz - sz*fy;
    double u2y = sz*fx - sx*fz;
    double u2z = sx*fy - sy*fx;
    GLfloat m[16] = {
        (GLfloat)sx,  (GLfloat)u2x, (GLfloat)-fx, 0.0f,
        (GLfloat)sy,  (GLfloat)u2y, (GLfloat)-fy, 0.0f,
        (GLfloat)sz,  (GLfloat)u2z, (GLfloat)-fz, 0.0f,
        (GLfloat)(-(sx*ex + sy*ey + sz*ez)),
        (GLfloat)(-(u2x*ex + u2y*ey + u2z*ez)),
        (GLfloat)(fx*ex + fy*ey + fz*ez),
        1.0f
    };
    glLoadMatrixf(m);
}
#define gluLookAt(ex,ey,ez,cx,cy,cz,ux,uy,uz) \
    wasm_glu_lookat((double)(ex),(double)(ey),(double)(ez), \
                    (double)(cx),(double)(cy),(double)(cz), \
                    (double)(ux),(double)(uy),(double)(uz))

/* === Immediate-mode primitive emulation ============================
   Emscripten's glEnd flush() aborts with "unsupported immediate mode N"
   for primitive modes GL_QUADS (7), GL_QUAD_STRIP (8), and GL_POLYGON (9).
   Strategy:
     - GL_QUAD_STRIP → GL_TRIANGLE_STRIP at glBegin (same vertex layout;
       diagonal of each quad differs but the surface covered is identical).
     - GL_POLYGON → GL_TRIANGLE_FAN at glBegin (callers' polygons are
       convex, so the fan triangulation is correct).
     - GL_QUADS: substitute mode to GL_TRIANGLES and buffer vertices
       (plus their per-vertex normal/texcoord/color state). Every 4
       buffered vertices form a quad V0-V1-V2-V3, which is replayed
       as 6 vertices forming triangles V0-V1-V2 and V0-V2-V3.

   The state setters (wasm_set_normal3f, etc.) always pass through to
   real GL *and* update g_wasm_cur. In QUADS mode the buffered snapshot
   captures the state at the moment glVertex* was called; replay re-emits
   the state setter calls before each replayed glVertex so per-vertex
   normals/texcoords/colors stay attached to the right corner. */

typedef struct {
    GLfloat x,  y,  z;
    GLfloat nx, ny, nz;
    GLfloat s,  t;
    GLfloat r,  g,  b,  a;
    char    has_n, has_t, has_c;
} wasm_v_t;

static GLenum   g_wasm_mode = 0;
static int      g_wasm_n    = 0;
static wasm_v_t g_wasm_cur;
static wasm_v_t g_wasm_buf[4];

/* Round-4 finding (probe data): under LEGACY_GL_EMULATION the
   GL_TEXTURE_2D binding silently "sticks" at the last id bound during
   the prior frame's GL_QUAD_STRIP path. glBindTexture calls issued
   right before a user-level glBegin(GL_QUADS) appear to be reordered
   or dropped by the emulator's immediate-mode buffering.

   Workaround: record every glBindTexture(GL_TEXTURE_2D, id) below and
   re-issue it inside wasm_flush_quad immediately before the per-quad
   real glBegin. The bind then lives strictly outside any Begin/End,
   right where the spec wants it, and is fresh on every quad. */
static GLuint g_wasm_last_tex_2d = 0;

/* The two helpers below run BEFORE the per-call #defines that wrap
   glBegin, glEnd, glColor3f/4f, glNormal3f, glTexCoord2f, glVertex3f.
   So the GL calls inside resolve to the real Emscripten entry points:
   no re-entry into the shims, no recursive state-machine corruption. */
static inline void wasm_replay(const wasm_v_t* v)
{
    if (v->has_n) glNormal3f(v->nx, v->ny, v->nz);
    if (v->has_t) glTexCoord2f(v->s, v->t);
    glVertex3f(v->x, v->y, v->z);
}

/* Flush four buffered quad vertices as a self-contained real
   Begin(TRIANGLES)/End. Per-quad uniform colour goes BEFORE glBegin so
   the implicit COLOR_MATERIAL to glMaterialfv evaluation happens in
   static state (legal) and not inside Begin/End (where
   LEGACY_GL_EMULATION drops the texture binding -- round-2 finding,
   round-4 confirmed via per-glBegin probe). */
static inline void wasm_flush_quad(void)
{
    /* Bind, colour, then draw — all material/state updates outside the
       real Begin/End so they don't fire COLOR_MATERIAL inside a Begin
       block (round-2 finding). Round-4 evidence shows this still does
       not restore texture sampling on the QUADS path — bind, texcoord,
       and colour all reach the draw correctly per probe, but
       LEGACY_GL_EMULATION's textured immediate-mode TRIANGLES/
       TRIANGLE_STRIP from this buffered path silently samples white.
       The shim leaves the path correct-by-construction; round-5 should
       go to a VBO rewrite for the static QUADS geometry. */
    if (g_wasm_last_tex_2d != 0) {
        glBindTexture(GL_TEXTURE_2D, g_wasm_last_tex_2d);
    }
    if (g_wasm_buf[0].has_c) {
        glColor4f(g_wasm_buf[0].r, g_wasm_buf[0].g,
                  g_wasm_buf[0].b, g_wasm_buf[0].a);
    }
    /* Emit quad V0,V1,V2,V3 as TRIANGLE_STRIP V0,V1,V3,V2 — 4 vertices
       and the matching primitive that does work for the wood lane via
       the non-buffered QUAD_STRIP path. */
    glBegin(GL_TRIANGLE_STRIP);
    wasm_replay(&g_wasm_buf[0]);
    wasm_replay(&g_wasm_buf[1]);
    wasm_replay(&g_wasm_buf[3]);
    wasm_replay(&g_wasm_buf[2]);
    glEnd();
}

static inline void wasm_glBindTexture(GLenum target, GLuint id)
{
    if (target == GL_TEXTURE_2D) g_wasm_last_tex_2d = id;
    glBindTexture(target, id);
}
#define glBindTexture wasm_glBindTexture

/* Defer the real glBegin for GL_QUADS — each completed 4-vertex quad is
   emitted in wasm_vertex3f as its own self-contained Begin(TRIANGLES)/
   End block, with the per-quad colour set just before the Begin. This
   keeps every material/colour update outside any real Begin/End and
   sidesteps the LEGACY_GL_EMULATION texture-binding clobber.

   GL_QUAD_STRIP and GL_POLYGON keep their direct mode substitution —
   they don't go through the buffer, and bowling.c doesn't set glColor
   inside those Begin/End blocks. */
static inline void wasm_glBegin(GLenum mode)
{
    g_wasm_mode = mode;
    g_wasm_n    = 0;
    /* g_wasm_cur is NOT reset here: callers sometimes set normal/colour
       once outside the Begin and rely on it persisting into the first
       quad's replay. The state setters below overwrite each field as
       the user touches it. */
    if (mode == GL_QUADS) return;
    GLenum real = mode;
    if      (mode == GL_QUAD_STRIP) real = GL_TRIANGLE_STRIP;
    else if (mode == GL_POLYGON)    real = GL_TRIANGLE_FAN;
    glBegin(real);
}
#define glBegin wasm_glBegin

static inline void wasm_glEnd(void)
{
    if (g_wasm_mode != GL_QUADS) glEnd();
    /* For QUADS, any incomplete trailing vertices (g_wasm_n < 4) are
       discarded — that input was already malformed GL. */
    g_wasm_mode = 0;
    g_wasm_n    = 0;
}
#define glEnd wasm_glEnd

/* State setters update g_wasm_cur unconditionally, but forward to real
   GL only when NOT in GL_QUADS mode. In QUADS mode the buffering pass
   emits 4 sets of (normal, texcoord, color) with no intervening
   glVertex3f — and Emscripten's "very limited" immediate-mode emulator
   was empirically observed to drop per-vertex texcoords on the GL_QUADS
   path when those leading state-change bursts were forwarded. By
   confining real-GL state changes to replay (one call per real vertex),
   Emscripten sees a clean state→vertex→state→vertex sequence. The
   QUAD_STRIP/TRIANGLE_STRIP path is unaffected (mode != GL_QUADS,
   state setters still pass through). */
static inline void wasm_set_normal3f(GLfloat x, GLfloat y, GLfloat z)
{
    g_wasm_cur.nx = x; g_wasm_cur.ny = y; g_wasm_cur.nz = z;
    g_wasm_cur.has_n = 1;
    if (g_wasm_mode != GL_QUADS) glNormal3f(x, y, z);
}
#define glNormal3f wasm_set_normal3f

static inline void wasm_set_texcoord2f(GLfloat s, GLfloat t)
{
    g_wasm_cur.s = s; g_wasm_cur.t = t;
    g_wasm_cur.has_t = 1;
    if (g_wasm_mode != GL_QUADS) glTexCoord2f(s, t);
}
#define glTexCoord2f wasm_set_texcoord2f

/* COLOR_MATERIAL emulation: forward color to glMaterialfv when
   color-material is on, but ONLY outside glBegin/glEnd. Calling
   glMaterialfv between Begin/End empirically breaks texture binding in
   Emscripten's LEGACY_GL_EMULATION.

   bowling.c sets colour inside Begin/End ~35 times so most surfaces
   never get a material update through this path; final.c does a
   per-frame glColor3f(1,1,1) reset to neutralize material before the
   scene helpers, and the lighting setup uses brighter ambient/diffuse
   on the wasm build so default-material surfaces stay visible. */
static inline void wasm_set_color3f(GLfloat r, GLfloat g, GLfloat b)
{
    g_wasm_cur.r = r; g_wasm_cur.g = g; g_wasm_cur.b = b; g_wasm_cur.a = 1.0f;
    g_wasm_cur.has_c = 1;
    if (g_wasm_mode != GL_QUADS) glColor3f(r, g, b);
    if (g_wasm_cm_on && g_wasm_mode == 0) {
        GLfloat c[4] = { r, g, b, 1.0f };
        glMaterialfv(g_wasm_cm_face, g_wasm_cm_mode, c);
    }
}
#define glColor3f wasm_set_color3f

static inline void wasm_set_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    g_wasm_cur.r = r; g_wasm_cur.g = g; g_wasm_cur.b = b; g_wasm_cur.a = a;
    g_wasm_cur.has_c = 1;
    if (g_wasm_mode != GL_QUADS) glColor4f(r, g, b, a);
    if (g_wasm_cm_on && g_wasm_mode == 0) {
        GLfloat c[4] = { r, g, b, a };
        glMaterialfv(g_wasm_cm_face, g_wasm_cm_mode, c);
    }
}
#define glColor4f wasm_set_color4f

static inline void wasm_vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    if (g_wasm_mode == GL_QUADS) {
        g_wasm_cur.x = x; g_wasm_cur.y = y; g_wasm_cur.z = z;
        g_wasm_buf[g_wasm_n++] = g_wasm_cur;
        if (g_wasm_n == 4) {
            wasm_flush_quad();
            g_wasm_n = 0;
        }
    } else {
        glVertex3f(x, y, z);
    }
}
#define glVertex3f wasm_vertex3f

static inline void wasm_vertex3fv(const GLfloat* v)
{
    wasm_vertex3f(v[0], v[1], v[2]);
}
#define glVertex3fv wasm_vertex3fv

/* glVertex3dv has no float-only equivalent that takes a double pointer;
   convert the 3 doubles into floats and forward through wasm_vertex3f so
   the QUADS buffer captures it too. */
static inline void wasm_glVertex3dv(const double v[3])
{
    wasm_vertex3f((float)v[0], (float)v[1], (float)v[2]);
}
#define glVertex3dv wasm_glVertex3dv

#endif /* __EMSCRIPTEN__ */

#endif /* WASM_COMPAT_H */
