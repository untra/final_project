#ifdef __EMSCRIPTEN__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CSCIx229.h"
#include "wasm_static_geom.h"

extern unsigned int floor_texture;
extern unsigned int mural_texture[4];
extern unsigned int wall_texture;
extern unsigned int cieling_texture;
extern unsigned int duct_texture;

typedef struct {
    GLfloat x, y, z;
    GLfloat u, v;
    GLfloat r, g, b, a;
    GLfloat nx, ny, nz;
} WasmStaticVertex;

typedef struct {
    GLuint texture;
    GLint first;
    GLsizei count;
    GLboolean use_texture;
} WasmStaticBatch;

typedef struct {
    double x, y, z;
    double sx, sy, sz;
    double rx, ry;
} WasmTransform;

static GLuint g_program;
static GLuint g_vbo;
static GLint g_u_projection = -1;
static GLint g_u_modelview = -1;
static GLint g_u_texture = -1;
static GLint g_u_use_texture = -1;
static GLint g_u_light_pos = -1;
static GLint g_u_light_ambient = -1;
static GLint g_u_light_diffuse = -1;
static GLint g_u_lighting = -1;
static WasmStaticBatch g_batches[20];
static int g_batch_count;
static int g_ready;

static WasmStaticVertex* g_vertices;
static size_t g_vertex_count;
static size_t g_vertex_cap;

static GLfloat g_light_pos[3]     = {0.0f, 30.0f, 20.0f};
static GLfloat g_light_ambient[3] = {0.55f, 0.55f, 0.55f};
static GLfloat g_light_diffuse[3] = {0.55f, 0.55f, 0.55f};
static int     g_lighting_enabled = 0;

static const char* k_vertex_shader =
    "precision mediump float;\n"
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_color;\n"
    "attribute vec3 a_normal;\n"
    "uniform mat4 u_projection;\n"
    "uniform mat4 u_modelview;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "varying vec3 v_normal_world;\n"
    "varying vec3 v_pos_world;\n"
    "void main(void) {\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "  v_normal_world = a_normal;\n"
    "  v_pos_world = a_pos;\n"
    "  gl_Position = u_projection * u_modelview * vec4(a_pos, 1.0);\n"
    "}\n";

static const char* k_fragment_shader =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform bool u_use_texture;\n"
    "uniform vec3 u_light_pos;\n"
    "uniform vec3 u_light_ambient;\n"
    "uniform vec3 u_light_diffuse;\n"
    "uniform bool u_lighting;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "varying vec3 v_normal_world;\n"
    "varying vec3 v_pos_world;\n"
    "void main(void) {\n"
    "  vec4 base = u_use_texture ? texture2D(u_texture, v_uv) * v_color : v_color;\n"
    "  if (!u_lighting) { gl_FragColor = base; return; }\n"
    "  vec3 N = normalize(v_normal_world);\n"
    "  vec3 L = normalize(u_light_pos - v_pos_world);\n"
    "  float ndotl = max(dot(N, L), 0.0);\n"
    "  vec3 lit = u_light_ambient * base.rgb + ndotl * u_light_diffuse * base.rgb;\n"
    "  gl_FragColor = vec4(lit, base.a);\n"
    "}\n";

static int reserve_vertices(size_t add)
{
    if (g_vertex_count + add <= g_vertex_cap) return 1;
    size_t next = g_vertex_cap ? g_vertex_cap * 2 : 4096;
    while (next < g_vertex_count + add) next *= 2;
    WasmStaticVertex* grown = (WasmStaticVertex*)realloc(g_vertices, next * sizeof(*grown));
    if (!grown) {
        printf("[wasm static geom] realloc failed for %lu vertices\n", (unsigned long)next);
        return 0;
    }
    g_vertices = grown;
    g_vertex_cap = next;
    return 1;
}

static WasmStaticBatch* begin_batch(GLuint texture)
{
    if (g_batch_count >= (int)(sizeof(g_batches) / sizeof(g_batches[0]))) return NULL;
    WasmStaticBatch* batch = &g_batches[g_batch_count++];
    batch->texture = texture;
    batch->first = (GLint)g_vertex_count;
    batch->count = 0;
    batch->use_texture = GL_TRUE;
    return batch;
}

static void transform_point(const WasmTransform* tr, const GLfloat in[3], GLfloat out[3])
{
    double x = in[0] * tr->sx;
    double y = in[1] * tr->sy;
    double z = in[2] * tr->sz;
    double cx = Cos(tr->rx);
    double sx = Sin(tr->rx);
    double cy = Cos(tr->ry);
    double sy = Sin(tr->ry);

    double y1 = y * cx - z * sx;
    double z1 = y * sx + z * cx;
    double x2 = x * cy + z1 * sy;
    double z2 = -x * sy + z1 * cy;

    out[0] = (GLfloat)(x2 + tr->x);
    out[1] = (GLfloat)(y1 + tr->y);
    out[2] = (GLfloat)(z2 + tr->z);
}

/* transform_point applies S → Rx → Ry → T. For a direction vector (normal),
   translation drops, rotations apply, and non-uniform scale must use the
   inverse-transpose (1/S on diagonal scale) followed by renormalize. */
static void transform_normal(const WasmTransform* tr, const GLfloat in[3], GLfloat out[3])
{
    double nx = (tr->sx != 0.0) ? (double)in[0] / tr->sx : 0.0;
    double ny = (tr->sy != 0.0) ? (double)in[1] / tr->sy : 0.0;
    double nz = (tr->sz != 0.0) ? (double)in[2] / tr->sz : 0.0;
    double cx = Cos(tr->rx);
    double sx = Sin(tr->rx);
    double cy = Cos(tr->ry);
    double sy = Sin(tr->ry);

    double y1 = ny * cx - nz * sx;
    double z1 = ny * sx + nz * cx;
    double x2 = nx * cy + z1 * sy;
    double z2 = -nx * sy + z1 * cy;

    double len = sqrt(x2 * x2 + y1 * y1 + z2 * z2);
    if (len > 1e-12) { x2 /= len; y1 /= len; z2 /= len; }

    out[0] = (GLfloat)x2;
    out[1] = (GLfloat)y1;
    out[2] = (GLfloat)z2;
}

static void push_vertex(WasmStaticBatch* batch,
                        const GLfloat p[3],
                        const GLfloat uv[2],
                        const GLfloat color[4],
                        const GLfloat normal_world[3])
{
    WasmStaticVertex* v;
    if (!reserve_vertices(1)) return;
    v = &g_vertices[g_vertex_count++];
    v->x = p[0]; v->y = p[1]; v->z = p[2];
    v->u = uv[0]; v->v = uv[1];
    v->r = color[0]; v->g = color[1]; v->b = color[2]; v->a = color[3];
    v->nx = normal_world[0]; v->ny = normal_world[1]; v->nz = normal_world[2];
    batch->count++;
}

static void append_quad(WasmStaticBatch* batch,
                        const WasmTransform* tr,
                        const GLfloat local[4][3],
                        const GLfloat uv[4][2],
                        const GLfloat color[4],
                        const GLfloat local_normal[3])
{
    GLfloat p[4][3];
    GLfloat nw[3];
    int i;
    for (i = 0; i < 4; i++) transform_point(tr, local[i], p[i]);
    transform_normal(tr, local_normal, nw);
    push_vertex(batch, p[0], uv[0], color, nw);
    push_vertex(batch, p[1], uv[1], color, nw);
    push_vertex(batch, p[2], uv[2], color, nw);
    push_vertex(batch, p[0], uv[0], color, nw);
    push_vertex(batch, p[2], uv[2], color, nw);
    push_vertex(batch, p[3], uv[3], color, nw);
}

/* Emit a single triangle from three local points; untextured. */
static void append_triangle(WasmStaticBatch* batch,
                            const WasmTransform* tr,
                            const GLfloat p0[3],
                            const GLfloat p1[3],
                            const GLfloat p2[3],
                            const GLfloat color[4],
                            const GLfloat local_normal[3])
{
    static const GLfloat zero_uv[2] = {0.0f, 0.0f};
    GLfloat t0[3], t1[3], t2[3];
    GLfloat nw[3];
    transform_point(tr, p0, t0);
    transform_point(tr, p1, t1);
    transform_point(tr, p2, t2);
    transform_normal(tr, local_normal, nw);
    push_vertex(batch, t0, zero_uv, color, nw);
    push_vertex(batch, t1, zero_uv, color, nw);
    push_vertex(batch, t2, zero_uv, color, nw);
}

/* Emit (n - 2) triangles as a fan around verts[0]. Used for
   GL_TRIANGLE_FAN and GL_POLYGON (convex). Untextured; zero UV. */
static void append_fan(WasmStaticBatch* batch,
                       const WasmTransform* tr,
                       const GLfloat verts[][3],
                       int n,
                       const GLfloat color[4],
                       const GLfloat local_normal[3])
{
    static const GLfloat zero_uv[2] = {0.0f, 0.0f};
    GLfloat center[3], a[3], b[3];
    GLfloat nw[3];
    int i;
    if (n < 3) return;
    transform_point(tr, verts[0], center);
    transform_normal(tr, local_normal, nw);
    for (i = 1; i + 1 < n; i++) {
        transform_point(tr, verts[i],   a);
        transform_point(tr, verts[i+1], b);
        push_vertex(batch, center, zero_uv, color, nw);
        push_vertex(batch, a,      zero_uv, color, nw);
        push_vertex(batch, b,      zero_uv, color, nw);
    }
}

/* Triangulate a GL_QUAD_STRIP-ordered local vertex list (n must be even,
   >= 4). Strip rule: quad i uses verts {2i, 2i+1, 2i+3, 2i+2} (v3/v2 swap).
   Untextured path; UVs pushed as zero. */
static void append_quad_strip(WasmStaticBatch* batch,
                              const WasmTransform* tr,
                              const GLfloat verts[][3],
                              int n,
                              const GLfloat color[4],
                              const GLfloat local_normal[3])
{
    static const GLfloat zero_uv[4][2] = {{0,0},{0,0},{0,0},{0,0}};
    int i;
    for (i = 0; i + 3 < n; i += 2) {
        GLfloat local[4][3];
        local[0][0] = verts[i  ][0]; local[0][1] = verts[i  ][1]; local[0][2] = verts[i  ][2];
        local[1][0] = verts[i+1][0]; local[1][1] = verts[i+1][1]; local[1][2] = verts[i+1][2];
        local[2][0] = verts[i+3][0]; local[2][1] = verts[i+3][1]; local[2][2] = verts[i+3][2];
        local[3][0] = verts[i+2][0]; local[3][1] = verts[i+2][1]; local[3][2] = verts[i+2][2];
        append_quad(batch, tr, local, zero_uv, color, local_normal);
    }
}

static void build_walls(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    /* Native wall() emits glNormal3f(1, 0, 0) before the ry rotation; the
       ry=180 right-wall instance correctly resolves to (-1, 0, 0) world. */
    static const GLfloat n[3] = {1.0f, 0.0f, 0.0f};
    static const GLfloat local[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 3.0f},
        {0.0f, 3.0f, 3.0f},
        {0.0f, 3.0f, 0.0f}
    };
    static const GLfloat uv[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    static const double right_z[] = {-30.0, 0.0, 30.0, 60.0, 90.0, 120.0};
    static const double left_z[] = {-60.0, -30.0, 0.0, 30.0, 60.0, 90.0};
    WasmStaticBatch* batch = begin_batch(wall_texture);
    int i;
    if (!batch) return;
    for (i = 0; i < 6; i++) {
        WasmTransform tr = {33.5, 0.0, right_z[i], 10.0, 12.0, 10.0, 0.0, 180.0};
        append_quad(batch, &tr, local, uv, color, n);
    }
    for (i = 0; i < 6; i++) {
        WasmTransform tr = {-110.5, 0.0, left_z[i], 10.0, 12.0, 10.0, 0.0, 0.0};
        append_quad(batch, &tr, local, uv, color, n);
    }
}

static void build_murals(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    /* Tilted mural face — native mural() emits (0, 0.8, -0.2). */
    static const GLfloat n[3] = {0.0f, 0.8f, -0.2f};
    static const GLfloat local[4][3] = {
        {3.0f, 0.5f, 12.0f},
        {0.0f, 0.5f, 12.0f},
        {0.0f, 3.0f, 11.8f},
        {3.0f, 3.0f, 11.8f}
    };
    static const GLfloat uv[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    static const double xs[] = {-2.0, -38.0, -74.0, -110.0};
    int i;
    for (i = 0; i < 4; i++) {
        WasmStaticBatch* batch = begin_batch(mural_texture[i]);
        WasmTransform tr = {xs[i], 0.0, -10.0, 12.0, 12.0, 10.0, 0.0, 0.0};
        if (batch) append_quad(batch, &tr, local, uv, color, n);
    }
}

static void build_floor_panels(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    /* Wood lane faces up; native alley() emits (0, 1, 0). */
    static const GLfloat lane_normal[3]  = {0.0f, 1.0f, 0.0f};
    /* Inter-lane floor panel uses the tilted normal from native floor_panel()
       in bowling.c — match verbatim for fidelity. */
    static const GLfloat panel_normal[3] = {0.0f, 0.8f, -0.2f};
    static const GLfloat panel_local[4][3] = {
        {3.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {3.0f, 0.0f, 1.0f}
    };
    static const GLfloat panel_uv[4][2] = {
        {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}
    };
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    static const double zs[] = {-10.0, -20.0, -30.0, -40.0};
    WasmStaticBatch* batch = begin_batch(floor_texture);
    int lane, j, i;
    if (!batch) return;
    for (lane = 0; lane < 4; lane++) {
        double alleys[2] = {lane_x[lane], lane_x[lane] + 19.0};
        for (j = 0; j < 2; j++) {
            WasmTransform lane_tr = {alleys[j], 0.0, 0.0, 10.0, 10.0, 9.75, 0.0, 0.0};
            for (i = 0; i < 12; i++) {
                GLfloat k = (GLfloat)i / 12.0f;
                GLfloat nv = (GLfloat)(i + 1) / 12.0f;
                GLfloat lane_local[4][3] = {
                    {0.0f, 0.0f, (GLfloat)i},
                    {1.0f, 0.0f, (GLfloat)i},
                    {1.0f, 0.0f, (GLfloat)(i + 1)},
                    {0.0f, 0.0f, (GLfloat)(i + 1)}
                };
                GLfloat lane_uv[4][2] = {
                    {0.0f, k}, {1.0f, k}, {1.0f, nv}, {0.0f, nv}
                };
                append_quad(batch, &lane_tr, lane_local, lane_uv, color, lane_normal);
            }
        }
        for (j = 0; j < 4; j++) {
            WasmTransform tr = {lane_x[lane] - 2.5, 0.0, zs[j], 12.0, 10.0, 10.0, 0.0, 0.0};
            append_quad(batch, &tr, panel_local, panel_uv, color, panel_normal);
        }
    }
}

static void build_ceilings(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    /* Ceiling faces downward toward the scene. */
    static const GLfloat n[3] = {0.0f, -1.0f, 0.0f};
    static const GLfloat local[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 4.0f},
        {0.0f, 0.0f, 4.0f}
    };
    static const GLfloat uv[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    WasmStaticBatch* batch = begin_batch(cieling_texture);
    int lane, i, j;
    if (!batch) return;
    for (lane = 0; lane < 4; lane++) {
        for (i = -1; i < 8; i++) {
            for (j = 0; j < 30; j++) {
                WasmTransform tr = {lane_x[lane] + (4.0 * i) + 1.0,
                                    30.0 + (j / 5),
                                    4.0 * j,
                                    1.0, 1.0, 1.0, 0.0, 0.0};
                append_quad(batch, &tr, local, uv, color, n);
            }
        }
    }
}

static void append_duct_cube(WasmStaticBatch* batch, const WasmTransform* tr)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    static const GLfloat n_bottom[3] = {0.0f, -1.0f,  0.0f};
    static const GLfloat n_front[3]  = {0.0f,  0.0f,  1.0f};
    static const GLfloat n_top[3]    = {0.0f,  1.0f,  0.0f};
    static const GLfloat n_back[3]   = {0.0f,  0.0f, -1.0f};
    static const GLfloat uv0[4][2] = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    static const GLfloat uv1[4][2] = {
        {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}
    };
    static const GLfloat bottom[4][3] = {
        {0.0f, 0.0f, 4.0f}, {4.0f, 0.0f, 4.0f}, {4.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}
    };
    static const GLfloat front[4][3] = {
        {0.0f, 0.0f, 4.0f}, {4.0f, 0.0f, 4.0f}, {4.0f, 4.0f, 4.0f}, {0.0f, 4.0f, 4.0f}
    };
    static const GLfloat top[4][3] = {
        {0.0f, 4.0f, 4.0f}, {4.0f, 4.0f, 4.0f}, {4.0f, 4.0f, 0.0f}, {0.0f, 4.0f, 0.0f}
    };
    static const GLfloat back[4][3] = {
        {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {4.0f, 4.0f, 0.0f}, {0.0f, 4.0f, 0.0f}
    };
    append_quad(batch, tr, bottom, uv0, color, n_bottom);
    append_quad(batch, tr, front,  uv1, color, n_front);
    append_quad(batch, tr, top,    uv0, color, n_top);
    append_quad(batch, tr, back,   uv1, color, n_back);
}

static void build_ducts(void)
{
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    WasmStaticBatch* batch = begin_batch(duct_texture);
    int lane, i;
    if (!batch) return;
    for (lane = 0; lane < 4; lane++) {
        for (i = -1; i < 8; i++) {
            WasmTransform tr = {lane_x[lane] + (4.0 * i) + 1.0,
                                30.0, -4.0,
                                1.0, 1.0, 1.0, 0.0, 0.0};
            append_duct_cube(batch, &tr);
        }
    }
}

static void build_dividers(void)
{
    static const GLfloat color[4] = {0.6f, 0.6f, 0.6f, 1.0f};
    static const GLfloat n_inner[3] = {-1.0f, 0.0f, 0.0f};
    static const GLfloat n_top[3]   = { 0.0f, 1.0f, 0.0f};
    static const GLfloat n_outer[3] = { 1.0f, 0.0f, 0.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    const double height = 0.5;
    const int length = 12;
    /* Two dividers per lane (bowling.c:804, 811):
       div1 at x+12.5 scale (4,1,10), div2 at x+31.5 scale (2,1,10). */
    struct { double dx, sx; } divs[2] = {
        {12.5, 4.0}, {31.5, 2.0}
    };
    WasmStaticBatch* batch = begin_batch(0);
    int lane, instance, i;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    for (lane = 0; lane < 4; lane++) {
        for (instance = 0; instance < 2; instance++) {
            WasmTransform tr = {lane_x[lane] + divs[instance].dx, 0.0, 0.0,
                                divs[instance].sx, 1.0, 10.0, 0.0, 0.0};
            for (i = 0; i < length; i++) {
                GLfloat inner[4][3] = {
                    {0.0f, 0.0f,            (GLfloat)i},
                    {0.0f, 0.0f,            (GLfloat)(i+1)},
                    {0.0f, (GLfloat)height, (GLfloat)i},
                    {0.0f, (GLfloat)height, (GLfloat)(i+1)}
                };
                GLfloat top[4][3] = {
                    {0.0f, (GLfloat)height, (GLfloat)i},
                    {0.0f, (GLfloat)height, (GLfloat)(i+1)},
                    {1.0f, (GLfloat)height, (GLfloat)i},
                    {1.0f, (GLfloat)height, (GLfloat)(i+1)}
                };
                GLfloat outer[4][3] = {
                    {1.0f, (GLfloat)height, (GLfloat)i},
                    {1.0f, (GLfloat)height, (GLfloat)(i+1)},
                    {1.0f, 0.0f,            (GLfloat)i},
                    {1.0f, 0.0f,            (GLfloat)(i+1)}
                };
                append_quad_strip(batch, &tr, inner, 4, color, n_inner);
                append_quad_strip(batch, &tr, top,   4, color, n_top);
                append_quad_strip(batch, &tr, outer, 4, color, n_outer);
            }
        }
    }
}

static void build_caps(void)
{
    static const GLfloat color[4] = {0.6f, 0.6f, 0.6f, 1.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    /* Strip verts in GL_QUAD_STRIP pair order, matching bowling.c:548-563.
       height=0.5 inlined; height/2 = 0.25. */
    static const GLfloat strip[12][3] = {
        {0.0f,  0.0f,  0.0f},
        {0.0f,  0.5f,  0.0f},
        {0.0f,  0.0f,  0.1f},
        {0.0f,  0.5f,  0.1f},
        {0.2f,  0.0f,  0.2f},
        {0.2f,  0.25f, 0.2f},
        {0.8f,  0.0f,  0.2f},
        {0.8f,  0.25f, 0.2f},
        {0.9f,  0.0f,  0.1f},
        {0.9f,  0.5f,  0.1f},
        {1.0f,  0.0f,  0.0f},
        {1.0f,  0.5f,  0.0f}
    };
    /* Top polygon (treated as fan from v0), CCW from bowling.c:568-573. */
    static const GLfloat top[6][3] = {
        {0.0f, 0.5f,  0.0f},
        {0.0f, 0.5f,  0.1f},
        {0.2f, 0.25f, 0.2f},
        {0.8f, 0.25f, 0.2f},
        {0.9f, 0.5f,  0.1f},
        {1.0f, 0.5f,  0.0f}
    };
    /* Three caps per lane (bowling.c:805, 807, 812). All ry=180. */
    struct { double dx, dy, dz, sx, sy, sz; } caps[3] = {
        {16.5, 0.0,   0.0, 4.0, 1.0, 10.0},
        {16.0, 3.0, -30.0, 3.0, 0.5,  5.0},
        {33.5, 0.0,   0.0, 2.0, 1.0,  5.0}
    };
    /* Cap is a bell-ish curved end-cap; native bowling.c cap() doesn't emit
       per-vertex normals, so we approximate the strip with a forward-leaning
       normal and the top fan with up. Faceted but matches native fidelity. */
    static const GLfloat n_strip[3] = {0.0f, 0.5f, -0.87f};
    static const GLfloat n_top[3]   = {0.0f, 1.0f,  0.0f};
    WasmStaticBatch* batch = begin_batch(0);
    int lane, c;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    for (lane = 0; lane < 4; lane++) {
        for (c = 0; c < 3; c++) {
            WasmTransform tr = {lane_x[lane] + caps[c].dx, caps[c].dy, caps[c].dz,
                                caps[c].sx, caps[c].sy, caps[c].sz, 0.0, 180.0};
            append_quad_strip(batch, &tr, strip, 12, color, n_strip);
            append_fan(batch, &tr, top, 6, color, n_top);
        }
    }
}

static void build_upcurves(void)
{
    static const GLfloat color[4] = {0.6f, 0.6f, 0.6f, 1.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    /* Precomputed curve profile from bowling.c:278-279. Hand-tuned
       smooth ramp, NOT from Cos/Sin macros — keep verbatim. */
    static const double icos[7] = {1.0, 0.985, 0.905, 0.707, 0.423, 0.174, 0.0};
    static const double isin[7] = {0.0, 0.174, 0.423, 0.707, 0.905, 0.985, 1.0};
    const double far_z = 2.0;
    /* Two upcurves per lane (bowling.c:803, 810), z=100. */
    struct { double dx, sx; } ups[2] = {
        {12.5, 4.0}, {31.5, 2.0}
    };
    WasmStaticBatch* batch = begin_batch(0);
    int lane, instance, i;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    for (lane = 0; lane < 4; lane++) {
        for (instance = 0; instance < 2; instance++) {
            WasmTransform tr = {lane_x[lane] + ups[instance].dx, 0.0, 100.0,
                                ups[instance].sx, 1.0, 10.0, 0.0, 0.0};
            /* Strip 1: front curved profile. 7 loop pairs + 1 trailing
               pair = 16 verts → 7 quads (last may be degenerate, matches
               native). */
            {
                static const GLfloat n_front[3] = {0.0f, 0.0f, -1.0f};
                GLfloat front[16][3];
                for (i = 0; i < 7; i++) {
                    double j = 5.5 * icos[i];
                    front[2*i  ][0] = 0.0f;
                    front[2*i  ][1] = (GLfloat)(6.0 - j);
                    front[2*i  ][2] = (GLfloat)isin[i];
                    front[2*i+1][0] = 1.0f;
                    front[2*i+1][1] = (GLfloat)(6.0 - j);
                    front[2*i+1][2] = (GLfloat)isin[i];
                }
                front[14][0] = 0.0f; front[14][1] = 6.0f; front[14][2] = 1.0f;
                front[15][0] = 1.0f; front[15][1] = 6.0f; front[15][2] = 1.0f;
                append_quad_strip(batch, &tr, front, 16, color, n_front);
            }
            /* Strip 2: right side wall. */
            {
                static const GLfloat n_right[3] = {1.0f, 0.0f, 0.0f};
                GLfloat right[16][3];
                for (i = 0; i < 7; i++) {
                    double j = 5.5 * icos[i];
                    right[2*i  ][0] = 1.0f;
                    right[2*i  ][1] = (GLfloat)(6.0 - j);
                    right[2*i  ][2] = (GLfloat)isin[i];
                    right[2*i+1][0] = 1.0f;
                    right[2*i+1][1] = (GLfloat)(6.0 - j);
                    right[2*i+1][2] = (GLfloat)far_z;
                }
                right[14][0] = 1.0f; right[14][1] = 6.0f; right[14][2] = 1.0f;
                right[15][0] = 1.0f; right[15][1] = 6.0f; right[15][2] = (GLfloat)far_z;
                append_quad_strip(batch, &tr, right, 16, color, n_right);
            }
            /* Strip 3: left side wall. */
            {
                static const GLfloat n_left[3] = {-1.0f, 0.0f, 0.0f};
                GLfloat left[16][3];
                for (i = 0; i < 7; i++) {
                    double j = 5.5 * icos[i];
                    left[2*i  ][0] = 0.0f;
                    left[2*i  ][1] = (GLfloat)(6.0 - j);
                    left[2*i  ][2] = (GLfloat)isin[i];
                    left[2*i+1][0] = 0.0f;
                    left[2*i+1][1] = (GLfloat)(6.0 - j);
                    left[2*i+1][2] = (GLfloat)far_z;
                }
                left[14][0] = 0.0f; left[14][1] = 6.0f; left[14][2] = 1.0f;
                left[15][0] = 0.0f; left[15][1] = 6.0f; left[15][2] = (GLfloat)far_z;
                append_quad_strip(batch, &tr, left, 16, color, n_left);
            }
        }
    }
}

static void build_ball_returns(void)
{
    static const GLfloat red[4]   = {0.6f, 0.0f, 0.0f, 1.0f};
    static const GLfloat green[4] = {0.0f, 0.6f, 0.0f, 1.0f};
    static const GLfloat blue[4]  = {0.0f, 0.0f, 0.6f, 1.0f};
    static const GLfloat gray[4]  = {0.6f, 0.6f, 0.6f, 1.0f};
    static const GLfloat black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    /* 8 base/platform/flat/top corner points; y values inlined
       (base=0.0, platform=0.3, flat=0.5, top=0.6). See bowling.c:338-353. */
    static const GLfloat base_a[3]     = {0.0f,  0.0f, 0.0f};
    static const GLfloat base_b[3]     = {0.0f,  0.0f, 0.6f};
    static const GLfloat base_c[3]     = {0.5f,  0.0f, 0.6f};
    static const GLfloat base_d[3]     = {0.5f,  0.0f, 0.0f};
    static const GLfloat platform_a[3] = {0.1f,  0.3f, 0.15f};
    static const GLfloat platform_b[3] = {0.1f,  0.3f, 0.5f};
    static const GLfloat platform_c[3] = {0.4f,  0.3f, 0.5f};
    static const GLfloat platform_d[3] = {0.4f,  0.3f, 0.15f};
    static const GLfloat flat_a[3]     = {0.05f, 0.5f, 0.25f};
    static const GLfloat flat_b[3]     = {0.05f, 0.5f, 0.5f};
    static const GLfloat flat_c[3]     = {0.45f, 0.5f, 0.5f};
    static const GLfloat flat_d[3]     = {0.45f, 0.5f, 0.25f};
    static const GLfloat top_a[3]      = {0.15f, 0.6f, 0.3f};
    static const GLfloat top_b[3]      = {0.15f, 0.6f, 0.6f};
    static const GLfloat top_c[3]      = {0.35f, 0.6f, 0.6f};
    static const GLfloat top_d[3]      = {0.35f, 0.6f, 0.3f};
    static const GLfloat zero_uv[4][2] = {{0,0},{0,0},{0,0},{0,0}};
    /* Per-face normals for the ball-return tower. */
    static const GLfloat n_right[3]   = { 1.0f, 0.0f,  0.0f};
    static const GLfloat n_left[3]    = {-1.0f, 0.0f,  0.0f};
    static const GLfloat n_back[3]    = { 0.0f, 0.0f, -1.0f};
    static const GLfloat n_up[3]      = { 0.0f, 1.0f,  0.0f};
    static const GLfloat n_forward[3] = { 0.0f, 0.0f,  1.0f};
    WasmStaticBatch* batch = begin_batch(0);
    int lane, i;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    for (lane = 0; lane < 4; lane++) {
        WasmTransform tr = {lane_x[lane] + 17.0, 0.0, -15.0,
                            10.0, 10.0, 10.0, 0.0, 180.0};
        /* Right strip: red. Pairs (base_a,b), (platform_a,b), (flat_a,b), (top_a,b). */
        {
            GLfloat strip[8][3];
            memcpy(strip[0], base_a,     sizeof(base_a));
            memcpy(strip[1], base_b,     sizeof(base_b));
            memcpy(strip[2], platform_a, sizeof(platform_a));
            memcpy(strip[3], platform_b, sizeof(platform_b));
            memcpy(strip[4], flat_a,     sizeof(flat_a));
            memcpy(strip[5], flat_b,     sizeof(flat_b));
            memcpy(strip[6], top_a,      sizeof(top_a));
            memcpy(strip[7], top_b,      sizeof(top_b));
            append_quad_strip(batch, &tr, strip, 8, red, n_right);
        }
        /* Left strip: green. Pairs (base_c,d), (platform_c,d), (flat_c,d), (top_c,d). */
        {
            GLfloat strip[8][3];
            memcpy(strip[0], base_c,     sizeof(base_c));
            memcpy(strip[1], base_d,     sizeof(base_d));
            memcpy(strip[2], platform_c, sizeof(platform_c));
            memcpy(strip[3], platform_d, sizeof(platform_d));
            memcpy(strip[4], flat_c,     sizeof(flat_c));
            memcpy(strip[5], flat_d,     sizeof(flat_d));
            memcpy(strip[6], top_c,      sizeof(top_c));
            memcpy(strip[7], top_d,      sizeof(top_d));
            append_quad_strip(batch, &tr, strip, 8, green, n_left);
        }
        /* Back strip: blue. Pairs (base_a,d), (platform_a,d), (flat_a,d), (top_a,d). */
        {
            GLfloat strip[8][3];
            memcpy(strip[0], base_a,     sizeof(base_a));
            memcpy(strip[1], base_d,     sizeof(base_d));
            memcpy(strip[2], platform_a, sizeof(platform_a));
            memcpy(strip[3], platform_d, sizeof(platform_d));
            memcpy(strip[4], flat_a,     sizeof(flat_a));
            memcpy(strip[5], flat_d,     sizeof(flat_d));
            memcpy(strip[6], top_a,      sizeof(top_a));
            memcpy(strip[7], top_d,      sizeof(top_d));
            append_quad_strip(batch, &tr, strip, 8, blue, n_back);
        }
        /* Top quad: gray. CCW (top_a, top_b, top_c, top_d). */
        {
            GLfloat local[4][3];
            memcpy(local[0], top_a, sizeof(top_a));
            memcpy(local[1], top_b, sizeof(top_b));
            memcpy(local[2], top_c, sizeof(top_c));
            memcpy(local[3], top_d, sizeof(top_d));
            append_quad(batch, &tr, local, zero_uv, gray, n_up);
        }
        /* Head lip: gray. (flat_b, flat_c, top_c, top_b). */
        {
            GLfloat local[4][3];
            memcpy(local[0], flat_b, sizeof(flat_b));
            memcpy(local[1], flat_c, sizeof(flat_c));
            memcpy(local[2], top_c,  sizeof(top_c));
            memcpy(local[3], top_b,  sizeof(top_b));
            append_quad(batch, &tr, local, zero_uv, gray, n_forward);
        }
        /* Base lip: gray. (base_b, base_c, platform_c, platform_b). */
        {
            GLfloat local[4][3];
            memcpy(local[0], base_b,     sizeof(base_b));
            memcpy(local[1], base_c,     sizeof(base_c));
            memcpy(local[2], platform_c, sizeof(platform_c));
            memcpy(local[3], platform_b, sizeof(platform_b));
            append_quad(batch, &tr, local, zero_uv, gray, n_forward);
        }
        /* Ball-return face: gray. (flat_b, flat_c, platform_c, platform_b). */
        {
            GLfloat local[4][3];
            memcpy(local[0], flat_b,     sizeof(flat_b));
            memcpy(local[1], flat_c,     sizeof(flat_c));
            memcpy(local[2], platform_c, sizeof(platform_c));
            memcpy(local[3], platform_b, sizeof(platform_b));
            append_quad(batch, &tr, local, zero_uv, gray, n_forward);
        }
        /* Ball-return center: black fan. Center (0.25, 0.4, 0.501); 25
           perimeter verts on circle of radius 0.1 in z=0.501 plane at
           angle 15° × i for i=0..24. Cos/Sin from CSCIx229.h are in degrees. */
        {
            GLfloat fan[26][3];
            fan[0][0] = 0.25f; fan[0][1] = 0.4f; fan[0][2] = 0.501f;
            for (i = 0; i <= 24; i++) {
                fan[i+1][0] = 0.25f + 0.1f * (GLfloat)Cos(15.0 * i);
                fan[i+1][1] = 0.4f  + 0.1f * (GLfloat)Sin(15.0 * i);
                fan[i+1][2] = 0.501f;
            }
            append_fan(batch, &tr, fan, 26, black, n_forward);
        }
        /* Left extension, bowling.c:452-458. */
        {
            GLfloat strip[6][3] = {
                {0.10f, 0.30f, 0.50f}, {0.10f, 0.30f, 1.50f},
                {0.10f, 0.32f, 0.50f}, {0.10f, 0.32f, 1.50f},
                {0.12f, 0.30f, 0.50f}, {0.12f, 0.30f, 1.50f}
            };
            append_quad_strip(batch, &tr, strip, 6, black, n_up);
        }
        /* Right extension, bowling.c:463-469. */
        {
            GLfloat strip[6][3] = {
                {0.40f, 0.30f, 0.50f}, {0.40f, 0.30f, 1.50f},
                {0.40f, 0.32f, 0.50f}, {0.40f, 0.32f, 1.50f},
                {0.38f, 0.30f, 0.50f}, {0.38f, 0.30f, 1.50f}
            };
            append_quad_strip(batch, &tr, strip, 6, black, n_up);
        }
        /* Middle extension, bowling.c:474-480. */
        {
            GLfloat strip[6][3] = {
                {0.27f, 0.30f, 0.50f}, {0.27f, 0.30f, 1.50f},
                {0.25f, 0.32f, 0.50f}, {0.25f, 0.32f, 1.50f},
                {0.23f, 0.30f, 0.50f}, {0.23f, 0.30f, 1.50f}
            };
            append_quad_strip(batch, &tr, strip, 6, black, n_up);
        }
        /* Downsplit, bowling.c:485-495. 10-vert quad strip — last two
           verts duplicate the first two to close the loop. */
        {
            GLfloat strip[10][3] = {
                {0.27f, 0.30f, 1.45f}, {0.27f, 0.30f, 1.50f},
                {0.25f, 0.00f, 1.35f}, {0.25f, 0.00f, 1.40f},
                {0.23f, 0.30f, 1.45f}, {0.23f, 0.30f, 1.50f},
                {0.25f, 0.00f, 1.35f}, {0.25f, 0.00f, 1.40f},
                {0.27f, 0.30f, 1.45f}, {0.27f, 0.30f, 1.50f}
            };
            append_quad_strip(batch, &tr, strip, 10, black, n_forward);
        }
        /* Downsplit faces, bowling.c:497-509. Two triangles. */
        {
            GLfloat front_p0[3] = {0.23f, 0.30f, 1.50f};
            GLfloat front_p1[3] = {0.27f, 0.30f, 1.50f};
            GLfloat front_p2[3] = {0.25f, 0.00f, 1.40f};
            GLfloat back_p0[3]  = {0.23f, 0.30f, 1.45f};
            GLfloat back_p1[3]  = {0.27f, 0.30f, 1.45f};
            GLfloat back_p2[3]  = {0.25f, 0.00f, 1.35f};
            append_triangle(batch, &tr, front_p0, front_p1, front_p2, black, n_forward);
            append_triangle(batch, &tr, back_p0,  back_p1,  back_p2,  black, n_back);
        }
        /* End face, bowling.c:511-522. Two coplanar quads at z=1.5. */
        {
            GLfloat q1[4][3] = {
                {0.40f, 0.30f, 1.50f},
                {0.40f, 0.32f, 1.50f},
                {0.25f, 0.32f, 1.50f},
                {0.25f, 0.30f, 1.50f}
            };
            GLfloat q2[4][3] = {
                {0.25f, 0.30f, 1.50f},
                {0.25f, 0.32f, 1.50f},
                {0.10f, 0.32f, 1.50f},
                {0.10f, 0.30f, 1.50f}
            };
            append_quad(batch, &tr, q1, zero_uv, black, n_forward);
            append_quad(batch, &tr, q2, zero_uv, black, n_forward);
        }
    }
}

/* Round 11: gutters + arrows.

   Port of bowling.c alley() at lines 627-697: a dark-gray U-channel gutter on
   each side of every lane, and red triangular arrow markers near the head
   pin. The native alley() function is gated out on WASM at final.c:382-393
   (because it's wrapped in double_lane() which also draws the wood lane,
   which is captured by build_floor_panels). The wood lane and inter-lane
   panels are already in batch 5; here we just add the gutters and arrows
   that were missed. */
static void build_gutters(void)
{
    static const GLfloat color[4]   = {0.2f, 0.2f, 0.2f, 1.0f};
    static const GLfloat n_inner[3] = {-1.0f, 0.0f, 0.0f};
    static const GLfloat n_bottom[3] = {0.0f, 1.0f, 0.0f};
    static const GLfloat n_outer[3] = { 1.0f, 0.0f, 0.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    WasmStaticBatch* batch = begin_batch(0);
    int lane, j, l, i;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    /* Per-alley transform matches build_floor_panels and bowling.c:821, 828:
       two alleys per double_lane at x=lane_x and x+19, scale (10, 10, 9.75). */
    for (lane = 0; lane < 4; lane++) {
        double alleys[2] = {lane_x[lane], lane_x[lane] + 19.0};
        for (j = 0; j < 2; j++) {
            WasmTransform tr = {alleys[j], 0.0, 0.0, 10.0, 10.0, 9.75, 0.0, 0.0};
            for (l = 0; l < 2; l++) {
                GLfloat xL = (GLfloat)l;
                GLfloat k  = (l == 0) ? -0.25f : 1.25f;
                for (i = 0; i < 12; i++) {
                    GLfloat s_inner[4][3] = {
                        {xL, 0.0f,  (GLfloat)i},
                        {xL, 0.0f,  (GLfloat)(i + 1)},
                        {xL, -0.1f, (GLfloat)i},
                        {xL, -0.1f, (GLfloat)(i + 1)}
                    };
                    GLfloat s_bot[4][3] = {
                        {xL, -0.1f, (GLfloat)i},
                        {xL, -0.1f, (GLfloat)(i + 1)},
                        {k,  -0.1f, (GLfloat)i},
                        {k,  -0.1f, (GLfloat)(i + 1)}
                    };
                    GLfloat s_outer[4][3] = {
                        {k, -0.1f, (GLfloat)i},
                        {k, -0.1f, (GLfloat)(i + 1)},
                        {k,  0.0f, (GLfloat)i},
                        {k,  0.0f, (GLfloat)(i + 1)}
                    };
                    append_quad_strip(batch, &tr, s_inner, 4, color, n_inner);
                    append_quad_strip(batch, &tr, s_bot,   4, color, n_bottom);
                    append_quad_strip(batch, &tr, s_outer, 4, color, n_outer);
                }
            }
        }
    }
}

static void build_arrows(void)
{
    static const GLfloat color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    static const GLfloat n_up[3]  = {0.0f, 1.0f, 0.0f};
    static const double lane_x[] = {0.0, -36.0, -72.0, -108.0};
    const GLfloat slightlyabove = 0.001f;
    WasmStaticBatch* batch = begin_batch(0);
    int lane, j, l, i;
    if (!batch) return;
    batch->use_texture = GL_FALSE;
    for (lane = 0; lane < 4; lane++) {
        double alleys[2] = {lane_x[lane], lane_x[lane] + 19.0};
        for (j = 0; j < 2; j++) {
            WasmTransform tr = {alleys[j], 0.0, 0.0, 10.0, 10.0, 9.75, 0.0, 0.0};
            for (l = 0; l < 2; l++) {
                for (i = 0; i < 5; i++) {
                    GLfloat xp, zp;
                    GLfloat p0[3], p1[3], p2[3];
                    zp = (GLfloat)(6 * l) + 0.03f * (GLfloat)i + 0.1f;
                    /* Right side. */
                    xp = 0.10f * (GLfloat)i + 0.10f;
                    p0[0] = xp;        p0[1] = slightlyabove; p0[2] = zp;
                    p1[0] = xp - 0.04f; p1[1] = 0.0f;          p1[2] = zp - 0.08f;
                    p2[0] = xp + 0.04f; p2[1] = 0.0f;          p2[2] = zp - 0.08f;
                    append_triangle(batch, &tr, p0, p1, p2, color, n_up);
                    /* Left-side mirror. */
                    xp = 1.0f - 0.10f * (GLfloat)i - 0.10f;
                    p0[0] = xp;        p0[1] = slightlyabove; p0[2] = zp;
                    p1[0] = xp - 0.04f; p1[1] = 0.0f;          p1[2] = zp - 0.08f;
                    p2[0] = xp + 0.04f; p2[1] = 0.0f;          p2[2] = zp - 0.08f;
                    append_triangle(batch, &tr, p0, p1, p2, color, n_up);
                }
            }
        }
    }
}

static GLuint compile_shader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    GLint ok = GL_FALSE;
    char log[1024];
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(log), &len, log);
        printf("[wasm static geom] shader compile failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static int build_program(void)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, k_vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, k_fragment_shader);
    GLint ok = GL_FALSE;
    char log[1024];
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glBindAttribLocation(g_program, 0, "a_pos");
    glBindAttribLocation(g_program, 1, "a_uv");
    glBindAttribLocation(g_program, 2, "a_color");
    glBindAttribLocation(g_program, 3, "a_normal");
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLsizei len = 0;
        glGetProgramInfoLog(g_program, sizeof(log), &len, log);
        printf("[wasm static geom] program link failed: %s\n", log);
        glDeleteProgram(g_program);
        g_program = 0;
        return 0;
    }
    g_u_projection = glGetUniformLocation(g_program, "u_projection");
    g_u_modelview = glGetUniformLocation(g_program, "u_modelview");
    g_u_texture = glGetUniformLocation(g_program, "u_texture");
    g_u_use_texture = glGetUniformLocation(g_program, "u_use_texture");
    g_u_light_pos = glGetUniformLocation(g_program, "u_light_pos");
    g_u_light_ambient = glGetUniformLocation(g_program, "u_light_ambient");
    g_u_light_diffuse = glGetUniformLocation(g_program, "u_light_diffuse");
    g_u_lighting = glGetUniformLocation(g_program, "u_lighting");
    return g_u_projection >= 0 && g_u_modelview >= 0 && g_u_texture >= 0 && g_u_use_texture >= 0;
}

void wasm_static_geom_init(void)
{
    if (g_ready) return;
    memset(g_batches, 0, sizeof(g_batches));
    g_batch_count = 0;
    g_vertex_count = 0;

    build_walls();
    build_murals();
    build_floor_panels();
    build_ceilings();
    build_ducts();
    build_dividers();
    build_caps();
    build_upcurves();
    build_ball_returns();
    build_gutters();
    build_arrows();

    if (!g_vertex_count || !build_program()) {
        printf("[wasm static geom] disabled; setup failed\n");
        free(g_vertices);
        g_vertices = NULL;
        g_vertex_cap = 0;
        g_vertex_count = 0;
        return;
    }

    glGenBuffers(1, &g_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(g_vertex_count * sizeof(g_vertices[0])),
                 g_vertices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    printf("[wasm static geom] ready: %lu vertices across %d texture batches\n",
           (unsigned long)g_vertex_count, g_batch_count);
    /* vB11 diag: dump first vertex of every batch so we can verify the VBO
       data matches the bowling.c geometry. Walls batch 0 starts at v[0],
       mural[0] batch 1 starts at v[72]. */
    {
        int b;
        for (b = 0; b < g_batch_count; b++) {
            int idx = g_batches[b].first;
            if (idx < 0 || (size_t)idx >= g_vertex_count) continue;
            const WasmStaticVertex* v = &g_vertices[idx];
            printf("[wasm static geom] batch %d first v: pos=(%.2f,%.2f,%.2f) uv=(%.2f,%.2f) color=(%.2f,%.2f,%.2f,%.2f) n=(%.2f,%.2f,%.2f)\n",
                   b, v->x, v->y, v->z, v->u, v->v, v->r, v->g, v->b, v->a, v->nx, v->ny, v->nz);
        }
    }
    g_ready = 1;
}

void wasm_static_geom_draw(void)
{
    GLfloat projection[16];
    GLfloat modelview[16];
    GLfloat prev_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    GLint prev_program = 0;
    GLint prev_array_buffer = 0;
    GLint prev_active_texture = GL_TEXTURE0;
    GLint prev_texture = 0;
    GLsizei stride = (GLsizei)sizeof(WasmStaticVertex);
    int i;

    if (!g_ready) return;

    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    glGetFloatv(GL_MODELVIEW_MATRIX, modelview);
    /* Round-7 fix: binding generic vertex attribute slot 2 (color) and then
       disabling it clobbers GL_CURRENT_COLOR to (0,0,0,0) under Emscripten's
       LEGACY_GL_EMULATION. Downstream immediate-mode draws (bowling pins —
       which call glColor3f only INSIDE glBegin and rely on the pre-Begin
       current color being sane) then render with alpha=0 and become
       invisible. Snapshot the current color so we can restore it on exit. */
    glGetFloatv(GL_CURRENT_COLOR, prev_color);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_array_buffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_texture);

    glUseProgram(g_program);
    glUniformMatrix4fv(g_u_projection, 1, GL_FALSE, projection);
    glUniformMatrix4fv(g_u_modelview, 1, GL_FALSE, modelview);
    glUniform1i(g_u_texture, 0);
    glUniform3fv(g_u_light_pos, 1, g_light_pos);
    glUniform3fv(g_u_light_ambient, 1, g_light_ambient);
    glUniform3fv(g_u_light_diffuse, 1, g_light_diffuse);
    glUniform1i(g_u_lighting, g_lighting_enabled);

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, r));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, nx));

    {
        static int once = 0;
        if (!once) {
            for (i = 0; i < g_batch_count; i++) {
                printf("[wasm static geom] batch %d: tex=%u first=%d count=%d\n",
                       i, g_batches[i].texture, g_batches[i].first, g_batches[i].count);
            }
            once = 1;
        }
    }
    for (i = 0; i < g_batch_count; i++) {
        GLenum err;
        if (g_batches[i].count <= 0) continue;
        glUniform1i(g_u_use_texture, g_batches[i].use_texture ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, g_batches[i].texture);
        glDrawArrays(GL_TRIANGLES, g_batches[i].first, g_batches[i].count);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            static int err_logged[20] = {0};
            if (i < 20 && !err_logged[i]) {
                printf("[wasm static geom] batch %d (tex=%u use_tex=%d) drew with glGetError=0x%x\n",
                       i, g_batches[i].texture, g_batches[i].use_texture, err);
                err_logged[i] = 1;
            }
        }
    }

    glDisableVertexAttribArray(3);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)prev_array_buffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_texture);
    glActiveTexture((GLenum)prev_active_texture);
    glUseProgram((GLuint)prev_program);
    /* Restore GL_CURRENT_COLOR — see snapshot above for why. */
    glColor4f(prev_color[0], prev_color[1], prev_color[2], prev_color[3]);
}

void wasm_static_geom_shutdown(void)
{
    if (g_vbo) glDeleteBuffers(1, &g_vbo);
    if (g_program) glDeleteProgram(g_program);
    free(g_vertices);
    g_vertices = NULL;
    g_vertex_cap = 0;
    g_vertex_count = 0;
    g_vbo = 0;
    g_program = 0;
    g_ready = 0;
}

void wasm_static_geom_set_light(const float pos[3], const float ambient[3], const float diffuse[3], int enabled)
{
    g_light_pos[0] = pos[0]; g_light_pos[1] = pos[1]; g_light_pos[2] = pos[2];
    g_light_ambient[0] = ambient[0]; g_light_ambient[1] = ambient[1]; g_light_ambient[2] = ambient[2];
    g_light_diffuse[0] = diffuse[0]; g_light_diffuse[1] = diffuse[1]; g_light_diffuse[2] = diffuse[2];
    g_lighting_enabled = enabled ? 1 : 0;
}

#endif
