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
static WasmStaticBatch g_batches[8];
static int g_batch_count;
static int g_ready;

static WasmStaticVertex* g_vertices;
static size_t g_vertex_count;
static size_t g_vertex_cap;

static const char* k_vertex_shader =
    "precision mediump float;\n"
    "attribute vec3 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_color;\n"
    "uniform mat4 u_projection;\n"
    "uniform mat4 u_modelview;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "void main(void) {\n"
    "  v_uv = a_uv;\n"
    "  v_color = a_color;\n"
    "  gl_Position = u_projection * u_modelview * vec4(a_pos, 1.0);\n"
    "}\n";

static const char* k_fragment_shader =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "uniform bool u_use_texture;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_color;\n"
    "void main(void) {\n"
    "  if (u_use_texture) {\n"
    "    gl_FragColor = texture2D(u_texture, v_uv) * v_color;\n"
    "  } else {\n"
    "    gl_FragColor = v_color;\n"
    "  }\n"
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

static void push_vertex(WasmStaticBatch* batch,
                        const GLfloat p[3],
                        const GLfloat uv[2],
                        const GLfloat color[4])
{
    WasmStaticVertex* v;
    if (!reserve_vertices(1)) return;
    v = &g_vertices[g_vertex_count++];
    v->x = p[0]; v->y = p[1]; v->z = p[2];
    v->u = uv[0]; v->v = uv[1];
    v->r = color[0]; v->g = color[1]; v->b = color[2]; v->a = color[3];
    batch->count++;
}

static void append_quad(WasmStaticBatch* batch,
                        const WasmTransform* tr,
                        const GLfloat local[4][3],
                        const GLfloat uv[4][2],
                        const GLfloat color[4])
{
    GLfloat p[4][3];
    int i;
    for (i = 0; i < 4; i++) transform_point(tr, local[i], p[i]);
    push_vertex(batch, p[0], uv[0], color);
    push_vertex(batch, p[1], uv[1], color);
    push_vertex(batch, p[2], uv[2], color);
    push_vertex(batch, p[0], uv[0], color);
    push_vertex(batch, p[2], uv[2], color);
    push_vertex(batch, p[3], uv[3], color);
}

static void build_walls(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
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
        append_quad(batch, &tr, local, uv, color);
    }
    for (i = 0; i < 6; i++) {
        WasmTransform tr = {-110.5, 0.0, left_z[i], 10.0, 12.0, 10.0, 0.0, 0.0};
        append_quad(batch, &tr, local, uv, color);
    }
}

static void build_murals(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
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
        if (batch) append_quad(batch, &tr, local, uv, color);
    }
}

static void build_floor_panels(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
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
                GLfloat n = (GLfloat)(i + 1) / 12.0f;
                GLfloat lane_local[4][3] = {
                    {0.0f, 0.0f, (GLfloat)i},
                    {1.0f, 0.0f, (GLfloat)i},
                    {1.0f, 0.0f, (GLfloat)(i + 1)},
                    {0.0f, 0.0f, (GLfloat)(i + 1)}
                };
                GLfloat lane_uv[4][2] = {
                    {0.0f, k}, {1.0f, k}, {1.0f, n}, {0.0f, n}
                };
                append_quad(batch, &lane_tr, lane_local, lane_uv, color);
            }
        }
        for (j = 0; j < 4; j++) {
            WasmTransform tr = {lane_x[lane] - 2.5, 0.0, zs[j], 12.0, 10.0, 10.0, 0.0, 0.0};
            append_quad(batch, &tr, panel_local, panel_uv, color);
        }
    }
}

static void build_ceilings(void)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
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
                append_quad(batch, &tr, local, uv, color);
            }
        }
    }
}

static void append_duct_cube(WasmStaticBatch* batch, const WasmTransform* tr)
{
    static const GLfloat color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
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
    append_quad(batch, tr, bottom, uv0, color);
    append_quad(batch, tr, front, uv1, color);
    append_quad(batch, tr, top, uv0, color);
    append_quad(batch, tr, back, uv1, color);
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
            printf("[wasm static geom] batch %d first v: pos=(%.2f,%.2f,%.2f) uv=(%.2f,%.2f) color=(%.2f,%.2f,%.2f,%.2f)\n",
                   b, v->x, v->y, v->z, v->u, v->v, v->r, v->g, v->b, v->a);
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

    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, x));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, u));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(WasmStaticVertex, r));

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
            static int err_logged[16] = {0};
            if (i < 16 && !err_logged[i]) {
                printf("[wasm static geom] batch %d (tex=%u use_tex=%d) drew with glGetError=0x%x\n",
                       i, g_batches[i].texture, g_batches[i].use_texture, err);
                err_logged[i] = 1;
            }
        }
    }

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

#endif
