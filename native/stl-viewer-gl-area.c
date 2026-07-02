#include <epoxy/gl.h>

#include "stl-viewer-gl-area.h"

#include <math.h>
#include <string.h>

#define DEFAULT_PITCH (-0.45f)
#define DEFAULT_YAW (0.65f)
#define DEFAULT_ZOOM (1.0f)
#define FLOAT_EPSILON (0.000001f)
#define PI_F (3.14159265358979323846f)

enum {
    SHADING_LIT = 0,
    SHADING_UNLIT = 1,
    SHADING_NORMALS = 2,
};

enum {
    MEASUREMENT_CHANGED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS];

typedef struct {
    float x;
    float y;
    float z;
} Vec3;

typedef struct {
    float px;
    float py;
    float pz;
    float nx;
    float ny;
    float nz;
} StlVertex;

struct _StlViewerGlArea {
    GtkGLArea parent_instance;

    GArray *vertices;
    GArray *edge_vertices;
    GArray *measure_vertices;
    char *model_path;
    char *status;
    char *measurement_text;
    guint triangle_count;

    GLuint program;
    GLuint mesh_vao;
    GLuint mesh_vbo;
    GLuint edge_vao;
    GLuint edge_vbo;
    GLuint measure_vao;
    GLuint measure_vbo;
    GLuint guide_vao;
    GLuint guide_vbo;
    GLint mvp_location;
    GLint normal_location;
    GLint color_location;
    GLint light_direction_location;
    GLint ambient_location;
    GLint exposure_location;
    GLint shading_mode_location;
    gboolean gl_ready;
    gboolean needs_upload;
    gboolean needs_edge_upload;
    gboolean needs_measure_upload;

    float pitch;
    float yaw;
    float zoom;
    float drag_start_pitch;
    float drag_start_yaw;
    float material_color[3];
    float background_color[3];
    float light_direction[3];
    float ambient;
    float exposure;
    int shading_mode;
    gboolean show_edges;
    gboolean measure_mode;
    gboolean have_measure_a;
    gboolean have_measure_b;
    Vec3 measure_a;
    Vec3 measure_b;
    float measurement_distance;
    float model_unit_scale;
    float bounds_width;
    float bounds_height;
    float bounds_depth;
};

G_DEFINE_TYPE(StlViewerGlArea, stl_viewer_gl_area, GTK_TYPE_GL_AREA)

static const char *desktop_vertex_shader_source =
    "#version 150\n"
    "in vec3 a_pos;\n"
    "in vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat3 u_normal_matrix;\n"
    "out vec3 v_normal;\n"
    "void main() {\n"
    "    v_normal = normalize(u_normal_matrix * a_normal);\n"
    "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char *desktop_fragment_shader_source =
    "#version 150\n"
    "in vec3 v_normal;\n"
    "uniform vec3 u_color;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform float u_ambient;\n"
    "uniform float u_exposure;\n"
    "uniform int u_shading_mode;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_normal);\n"
    "    if (!gl_FrontFacing) n = -n;\n"
    "    vec3 color;\n"
    "    if (u_shading_mode == 2) {\n"
    "        color = n * 0.5 + 0.5;\n"
    "    } else if (u_shading_mode == 1) {\n"
    "        color = u_color;\n"
    "    } else {\n"
    "        vec3 light = normalize(u_light_dir);\n"
    "        float diffuse = max(dot(n, light), 0.0);\n"
    "        float fill = max(dot(n, normalize(vec3(-light.x, -0.35, abs(light.z)))), 0.0);\n"
    "        color = u_color * (u_ambient + (0.82 - u_ambient) * diffuse + 0.14 * fill);\n"
    "    }\n"
    "    color *= u_exposure;\n"
    "    frag_color = vec4(color, 1.0);\n"
    "}\n";

static const char *gles_vertex_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec3 a_pos;\n"
    "in vec3 a_normal;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat3 u_normal_matrix;\n"
    "out vec3 v_normal;\n"
    "void main() {\n"
    "    v_normal = normalize(u_normal_matrix * a_normal);\n"
    "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
    "}\n";

static const char *gles_fragment_shader_source =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec3 v_normal;\n"
    "uniform vec3 u_color;\n"
    "uniform vec3 u_light_dir;\n"
    "uniform float u_ambient;\n"
    "uniform float u_exposure;\n"
    "uniform int u_shading_mode;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec3 n = normalize(v_normal);\n"
    "    if (!gl_FrontFacing) n = -n;\n"
    "    vec3 color;\n"
    "    if (u_shading_mode == 2) {\n"
    "        color = n * 0.5 + 0.5;\n"
    "    } else if (u_shading_mode == 1) {\n"
    "        color = u_color;\n"
    "    } else {\n"
    "        vec3 light = normalize(u_light_dir);\n"
    "        float diffuse = max(dot(n, light), 0.0);\n"
    "        float fill = max(dot(n, normalize(vec3(-light.x, -0.35, abs(light.z)))), 0.0);\n"
    "        color = u_color * (u_ambient + (0.82 - u_ambient) * diffuse + 0.14 * fill);\n"
    "    }\n"
    "    color *= u_exposure;\n"
    "    frag_color = vec4(color, 1.0);\n"
    "}\n";

static float
clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static Vec3
vec3_sub(Vec3 a, Vec3 b)
{
    Vec3 out = {a.x - b.x, a.y - b.y, a.z - b.z};
    return out;
}

static Vec3
vec3_cross(Vec3 a, Vec3 b)
{
    Vec3 out = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
    return out;
}

static float
vec3_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3
vec3_add(Vec3 a, Vec3 b)
{
    Vec3 out = {a.x + b.x, a.y + b.y, a.z + b.z};
    return out;
}

static Vec3
vec3_scale(Vec3 v, float scale)
{
    Vec3 out = {v.x * scale, v.y * scale, v.z * scale};
    return out;
}

static float
vec3_length(Vec3 v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec3
vec3_normalize(Vec3 v)
{
    float length = vec3_length(v);
    if (length < FLOAT_EPSILON) {
        Vec3 fallback = {0.0f, 0.0f, 1.0f};
        return fallback;
    }

    Vec3 out = {v.x / length, v.y / length, v.z / length};
    return out;
}

static void
set_status(StlViewerGlArea *self, const char *status)
{
    g_free(self->status);
    self->status = g_strdup(status != NULL ? status : "");
}

static void
mat4_identity(float m[16])
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void
mat4_multiply(float out[16], const float a[16], const float b[16])
{
    float result[16];

    for (int col = 0; col < 4; col++) {
        for (int row = 0; row < 4; row++) {
            result[col * 4 + row] =
                a[0 * 4 + row] * b[col * 4 + 0] +
                a[1 * 4 + row] * b[col * 4 + 1] +
                a[2 * 4 + row] * b[col * 4 + 2] +
                a[3 * 4 + row] * b[col * 4 + 3];
        }
    }

    memcpy(out, result, sizeof(result));
}

static void
mat4_transform_vec4(const float m[16], const float v[4], float out[4])
{
    for (int row = 0; row < 4; row++) {
        out[row] =
            m[0 * 4 + row] * v[0] +
            m[1 * 4 + row] * v[1] +
            m[2 * 4 + row] * v[2] +
            m[3 * 4 + row] * v[3];
    }
}

static gboolean
mat4_invert(const float m[16], float inv_out[16])
{
    float inv[16];
    float det;

    inv[0] = m[5]  * m[10] * m[15] -
             m[5]  * m[11] * m[14] -
             m[9]  * m[6]  * m[15] +
             m[9]  * m[7]  * m[14] +
             m[13] * m[6]  * m[11] -
             m[13] * m[7]  * m[10];

    inv[4] = -m[4]  * m[10] * m[15] +
              m[4]  * m[11] * m[14] +
              m[8]  * m[6]  * m[15] -
              m[8]  * m[7]  * m[14] -
              m[12] * m[6]  * m[11] +
              m[12] * m[7]  * m[10];

    inv[8] = m[4]  * m[9] * m[15] -
             m[4]  * m[11] * m[13] -
             m[8]  * m[5] * m[15] +
             m[8]  * m[7] * m[13] +
             m[12] * m[5] * m[11] -
             m[12] * m[7] * m[9];

    inv[12] = -m[4]  * m[9] * m[14] +
               m[4]  * m[10] * m[13] +
               m[8]  * m[5] * m[14] -
               m[8]  * m[6] * m[13] -
               m[12] * m[5] * m[10] +
               m[12] * m[6] * m[9];

    inv[1] = -m[1]  * m[10] * m[15] +
              m[1]  * m[11] * m[14] +
              m[9]  * m[2] * m[15] -
              m[9]  * m[3] * m[14] -
              m[13] * m[2] * m[11] +
              m[13] * m[3] * m[10];

    inv[5] = m[0]  * m[10] * m[15] -
             m[0]  * m[11] * m[14] -
             m[8]  * m[2] * m[15] +
             m[8]  * m[3] * m[14] +
             m[12] * m[2] * m[11] -
             m[12] * m[3] * m[10];

    inv[9] = -m[0]  * m[9] * m[15] +
              m[0]  * m[11] * m[13] +
              m[8]  * m[1] * m[15] -
              m[8]  * m[3] * m[13] -
              m[12] * m[1] * m[11] +
              m[12] * m[3] * m[9];

    inv[13] = m[0]  * m[9] * m[14] -
              m[0]  * m[10] * m[13] -
              m[8]  * m[1] * m[14] +
              m[8]  * m[2] * m[13] +
              m[12] * m[1] * m[10] -
              m[12] * m[2] * m[9];

    inv[2] = m[1]  * m[6] * m[15] -
             m[1]  * m[7] * m[14] -
             m[5]  * m[2] * m[15] +
             m[5]  * m[3] * m[14] +
             m[13] * m[2] * m[7] -
             m[13] * m[3] * m[6];

    inv[6] = -m[0]  * m[6] * m[15] +
              m[0]  * m[7] * m[14] +
              m[4]  * m[2] * m[15] -
              m[4]  * m[3] * m[14] -
              m[12] * m[2] * m[7] +
              m[12] * m[3] * m[6];

    inv[10] = m[0]  * m[5] * m[15] -
              m[0]  * m[7] * m[13] -
              m[4]  * m[1] * m[15] +
              m[4]  * m[3] * m[13] +
              m[12] * m[1] * m[7] -
              m[12] * m[3] * m[5];

    inv[14] = -m[0]  * m[5] * m[14] +
               m[0]  * m[6] * m[13] +
               m[4]  * m[1] * m[14] -
               m[4]  * m[2] * m[13] -
               m[12] * m[1] * m[6] +
               m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] +
              m[1] * m[7] * m[10] +
              m[5] * m[2] * m[11] -
              m[5] * m[3] * m[10] -
              m[9] * m[2] * m[7] +
              m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] -
             m[0] * m[7] * m[10] -
             m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] -
             m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] +
               m[0] * m[7] * m[9] +
               m[4] * m[1] * m[11] -
               m[4] * m[3] * m[9] -
               m[8] * m[1] * m[7] +
               m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] -
              m[0] * m[6] * m[9] -
              m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (fabsf(det) < FLOAT_EPSILON)
        return FALSE;

    det = 1.0f / det;
    for (int i = 0; i < 16; i++)
        inv_out[i] = inv[i] * det;

    return TRUE;
}

static void
mat4_translation(float out[16], float x, float y, float z)
{
    mat4_identity(out);
    out[12] = x;
    out[13] = y;
    out[14] = z;
}

static void
mat4_rotation_x(float out[16], float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    mat4_identity(out);
    out[5] = c;
    out[6] = s;
    out[9] = -s;
    out[10] = c;
}

static void
mat4_rotation_y(float out[16], float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);

    mat4_identity(out);
    out[0] = c;
    out[2] = -s;
    out[8] = s;
    out[10] = c;
}

static void
mat4_perspective(float out[16], float fov_y, float aspect, float near_plane, float far_plane)
{
    float f = 1.0f / tanf(fov_y * 0.5f);

    memset(out, 0, sizeof(float) * 16);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (far_plane + near_plane) / (near_plane - far_plane);
    out[11] = -1.0f;
    out[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
}

static void
normal_matrix_from_model(float out[9], const float model[16])
{
    out[0] = model[0];
    out[1] = model[1];
    out[2] = model[2];
    out[3] = model[4];
    out[4] = model[5];
    out[5] = model[6];
    out[6] = model[8];
    out[7] = model[9];
    out[8] = model[10];
}

static void
update_bounds(Vec3 v, gboolean *have_bounds, Vec3 *min, Vec3 *max)
{
    if (!*have_bounds) {
        *min = v;
        *max = v;
        *have_bounds = TRUE;
        return;
    }

    min->x = MIN(min->x, v.x);
    min->y = MIN(min->y, v.y);
    min->z = MIN(min->z, v.z);
    max->x = MAX(max->x, v.x);
    max->y = MAX(max->y, v.y);
    max->z = MAX(max->z, v.z);
}

static void
append_triangle(GArray *vertices,
                Vec3 a,
                Vec3 b,
                Vec3 c,
                Vec3 normal,
                gboolean *have_bounds,
                Vec3 *min,
                Vec3 *max)
{
    if (vec3_length(normal) < FLOAT_EPSILON)
        normal = vec3_cross(vec3_sub(b, a), vec3_sub(c, a));
    normal = vec3_normalize(normal);

    StlVertex tri[3] = {
        {a.x, a.y, a.z, normal.x, normal.y, normal.z},
        {b.x, b.y, b.z, normal.x, normal.y, normal.z},
        {c.x, c.y, c.z, normal.x, normal.y, normal.z},
    };

    g_array_append_vals(vertices, tri, 3);
    update_bounds(a, have_bounds, min, max);
    update_bounds(b, have_bounds, min, max);
    update_bounds(c, have_bounds, min, max);
}

static float
normalize_mesh(GArray *vertices, Vec3 min, Vec3 max)
{
    Vec3 center = {
        (min.x + max.x) * 0.5f,
        (min.y + max.y) * 0.5f,
        (min.z + max.z) * 0.5f,
    };
    float radius = 0.0f;

    for (guint i = 0; i < vertices->len; i++) {
        StlVertex *v = &g_array_index(vertices, StlVertex, i);
        Vec3 p = {v->px - center.x, v->py - center.y, v->pz - center.z};
        radius = MAX(radius, vec3_length(p));
    }

    if (radius < FLOAT_EPSILON)
        radius = 1.0f;

    for (guint i = 0; i < vertices->len; i++) {
        StlVertex *v = &g_array_index(vertices, StlVertex, i);
        v->px = (v->px - center.x) / radius;
        v->py = (v->py - center.y) / radius;
        v->pz = (v->pz - center.z) / radius;
    }

    return radius;
}

static guint32
read_u32_le(const guint8 *data)
{
    guint32 value;
    memcpy(&value, data, sizeof(value));
    return GUINT32_FROM_LE(value);
}

static float
read_float_le(const guint8 *data)
{
    guint32 value = read_u32_le(data);
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static gboolean
parse_vec3(const char *text, Vec3 *out)
{
    char *end = NULL;

    while (g_ascii_isspace(*text)) text++;
    out->x = (float)g_ascii_strtod(text, &end);
    if (end == text) return FALSE;

    text = end;
    while (g_ascii_isspace(*text)) text++;
    out->y = (float)g_ascii_strtod(text, &end);
    if (end == text) return FALSE;

    text = end;
    while (g_ascii_isspace(*text)) text++;
    out->z = (float)g_ascii_strtod(text, &end);
    if (end == text) return FALSE;

    return TRUE;
}

static gboolean
starts_with_ascii_solid(const guint8 *data, gsize length)
{
    if (length < 5) return FALSE;
    return g_ascii_strncasecmp((const char *)data, "solid", 5) == 0;
}

static gboolean
looks_like_binary_stl(const guint8 *data, gsize length, guint32 *triangle_count)
{
    if (length < 84) return FALSE;

    guint32 count = read_u32_le(data + 80);
    guint64 expected = 84ull + ((guint64)count * 50ull);
    *triangle_count = count;

    if (expected == length) return TRUE;
    if (expected < length && !starts_with_ascii_solid(data, length)) return TRUE;

    return FALSE;
}

static gboolean
parse_binary_stl(const guint8 *data,
                 gsize length,
                 GArray *vertices,
                 Vec3 *min,
                 Vec3 *max,
                 char **error_message)
{
    guint32 triangle_count = 0;
    if (!looks_like_binary_stl(data, length, &triangle_count)) {
        *error_message = g_strdup("File is not a valid binary STL");
        return FALSE;
    }

    guint64 expected = 84ull + ((guint64)triangle_count * 50ull);
    if (expected > length) {
        *error_message = g_strdup("Binary STL is truncated");
        return FALSE;
    }

    gboolean have_bounds = FALSE;
    const guint8 *cursor = data + 84;

    for (guint32 i = 0; i < triangle_count; i++) {
        Vec3 normal = {
            read_float_le(cursor),
            read_float_le(cursor + 4),
            read_float_le(cursor + 8),
        };
        cursor += 12;

        Vec3 a = {
            read_float_le(cursor),
            read_float_le(cursor + 4),
            read_float_le(cursor + 8),
        };
        cursor += 12;

        Vec3 b = {
            read_float_le(cursor),
            read_float_le(cursor + 4),
            read_float_le(cursor + 8),
        };
        cursor += 12;

        Vec3 c = {
            read_float_le(cursor),
            read_float_le(cursor + 4),
            read_float_le(cursor + 8),
        };
        cursor += 12;

        cursor += 2;

        append_triangle(vertices, a, b, c, normal, &have_bounds, min, max);
    }

    if (vertices->len == 0) {
        *error_message = g_strdup("STL contains no triangles");
        return FALSE;
    }

    return TRUE;
}

static gboolean
parse_ascii_stl(const char *contents,
                GArray *vertices,
                Vec3 *min,
                Vec3 *max,
                char **error_message)
{
    gchar **lines = g_strsplit(contents, "\n", -1);
    gboolean have_bounds = FALSE;
    Vec3 normal = {0.0f, 0.0f, 0.0f};
    Vec3 triangle[3];
    int vertex_count = 0;

    for (guint i = 0; lines[i] != NULL; i++) {
        char *line = g_strstrip(lines[i]);

        if (g_ascii_strncasecmp(line, "facet normal", 12) == 0) {
            if (!parse_vec3(line + 12, &normal))
                normal = (Vec3){0.0f, 0.0f, 0.0f};
            vertex_count = 0;
        } else if (g_ascii_strncasecmp(line, "vertex", 6) == 0) {
            Vec3 vertex;
            if (!parse_vec3(line + 6, &vertex))
                continue;

            triangle[vertex_count++] = vertex;
            if (vertex_count == 3) {
                append_triangle(vertices,
                                triangle[0],
                                triangle[1],
                                triangle[2],
                                normal,
                                &have_bounds,
                                min,
                                max);
                vertex_count = 0;
            }
        }
    }

    g_strfreev(lines);

    if (vertices->len == 0) {
        *error_message = g_strdup("ASCII STL contains no triangles");
        return FALSE;
    }

    return TRUE;
}

static gboolean
load_stl_vertices(const char *path,
                  GArray **out_vertices,
                  guint *out_triangle_count,
                  float *out_model_unit_scale,
                  float *out_bounds_width,
                  float *out_bounds_height,
                  float *out_bounds_depth,
                  char **error_message)
{
    char *contents = NULL;
    gsize length = 0;
    GError *error = NULL;

    if (!g_file_get_contents(path, &contents, &length, &error)) {
        *error_message = g_strdup(error->message);
        g_error_free(error);
        return FALSE;
    }

    GArray *vertices = g_array_new(FALSE, FALSE, sizeof(StlVertex));
    Vec3 min = {0.0f, 0.0f, 0.0f};
    Vec3 max = {0.0f, 0.0f, 0.0f};
    guint32 binary_count = 0;
    gboolean ok;

    if (looks_like_binary_stl((const guint8 *)contents, length, &binary_count)) {
        ok = parse_binary_stl((const guint8 *)contents, length, vertices, &min, &max, error_message);
    } else {
        ok = parse_ascii_stl(contents, vertices, &min, &max, error_message);
    }

    g_free(contents);

    if (!ok) {
        g_array_unref(vertices);
        return FALSE;
    }

    *out_model_unit_scale = normalize_mesh(vertices, min, max);
    *out_bounds_width = max.x - min.x;
    *out_bounds_height = max.y - min.y;
    *out_bounds_depth = max.z - min.z;

    *out_triangle_count = vertices->len / 3;
    *out_vertices = vertices;
    return TRUE;
}

static GLuint
compile_shader(GLenum shader_type, const char *source)
{
    GLuint shader = glCreateShader(shader_type);
    GLint status = GL_FALSE;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
        char log[1024];
        GLsizei length = 0;
        glGetShaderInfoLog(shader, sizeof(log), &length, log);
        g_warning("shader compile failed: %.*s", length, log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint
create_program(gboolean use_gles)
{
    const char *vertex_source = use_gles ? gles_vertex_shader_source : desktop_vertex_shader_source;
    const char *fragment_source = use_gles ? gles_fragment_shader_source : desktop_fragment_shader_source;
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    GLuint program = 0;
    GLint status = GL_FALSE;

    if (vertex_shader == 0 || fragment_shader == 0)
        goto out;

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glBindAttribLocation(program, 0, "a_pos");
    glBindAttribLocation(program, 1, "a_normal");
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        char log[1024];
        GLsizei length = 0;
        glGetProgramInfoLog(program, sizeof(log), &length, log);
        g_warning("program link failed: %.*s", length, log);
        glDeleteProgram(program);
        program = 0;
    }

out:
    if (vertex_shader != 0) glDeleteShader(vertex_shader);
    if (fragment_shader != 0) glDeleteShader(fragment_shader);
    return program;
}

static void
configure_vertex_array(GLuint vao, GLuint vbo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StlVertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(StlVertex), (void *)(sizeof(float) * 3));
    glBindVertexArray(0);
}

static void
upload_mesh(StlViewerGlArea *self)
{
    if (!self->gl_ready || self->mesh_vbo == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, self->mesh_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 self->vertices->len * sizeof(StlVertex),
                 self->vertices->data,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    self->needs_upload = FALSE;
}

static void
append_line(GArray *vertices, Vec3 a, Vec3 b)
{
    StlVertex line[2] = {
        {a.x, a.y, a.z, 0.0f, 0.0f, 1.0f},
        {b.x, b.y, b.z, 0.0f, 0.0f, 1.0f},
    };
    g_array_append_vals(vertices, line, 2);
}

static Vec3
vertex_position(const StlVertex *vertex)
{
    Vec3 out = {vertex->px, vertex->py, vertex->pz};
    return out;
}

static void
rebuild_edge_vertices(StlViewerGlArea *self)
{
    g_array_set_size(self->edge_vertices, 0);

    for (guint i = 0; i + 2 < self->vertices->len; i += 3) {
        StlVertex *va = &g_array_index(self->vertices, StlVertex, i);
        StlVertex *vb = &g_array_index(self->vertices, StlVertex, i + 1);
        StlVertex *vc = &g_array_index(self->vertices, StlVertex, i + 2);
        Vec3 a = vertex_position(va);
        Vec3 b = vertex_position(vb);
        Vec3 c = vertex_position(vc);

        append_line(self->edge_vertices, a, b);
        append_line(self->edge_vertices, b, c);
        append_line(self->edge_vertices, c, a);
    }

    self->needs_edge_upload = TRUE;
}

static void
append_marker(GArray *vertices, Vec3 p)
{
    const float size = 0.035f;
    Vec3 x1 = {p.x - size, p.y, p.z};
    Vec3 x2 = {p.x + size, p.y, p.z};
    Vec3 y1 = {p.x, p.y - size, p.z};
    Vec3 y2 = {p.x, p.y + size, p.z};
    Vec3 z1 = {p.x, p.y, p.z - size};
    Vec3 z2 = {p.x, p.y, p.z + size};

    append_line(vertices, x1, x2);
    append_line(vertices, y1, y2);
    append_line(vertices, z1, z2);
}

static void
update_measurement_text(StlViewerGlArea *self)
{
    g_free(self->measurement_text);

    if (!self->have_measure_a) {
        self->measurement_text = g_strdup(self->measure_mode ?
            "Measure: click the first point" : "Measure: inactive");
    } else if (!self->have_measure_b) {
        self->measurement_text = g_strdup("Measure: click the second point");
    } else {
        self->measurement_text = g_strdup_printf("Measure: %.4g units",
                                                 self->measurement_distance);
    }
}

static void
rebuild_measure_vertices(StlViewerGlArea *self)
{
    g_array_set_size(self->measure_vertices, 0);

    if (self->have_measure_a)
        append_marker(self->measure_vertices, self->measure_a);
    if (self->have_measure_b) {
        append_marker(self->measure_vertices, self->measure_b);
        append_line(self->measure_vertices, self->measure_a, self->measure_b);
    }

    self->needs_measure_upload = TRUE;
}

static void
upload_edges(StlViewerGlArea *self)
{
    if (!self->gl_ready || self->edge_vbo == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, self->edge_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 self->edge_vertices->len * sizeof(StlVertex),
                 self->edge_vertices->data,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    self->needs_edge_upload = FALSE;
}

static void
upload_measurement(StlViewerGlArea *self)
{
    if (!self->gl_ready || self->measure_vbo == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, self->measure_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 self->measure_vertices->len * sizeof(StlVertex),
                 self->measure_vertices->data,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    self->needs_measure_upload = FALSE;
}

static void
create_guide_geometry(StlViewerGlArea *self)
{
    static const StlVertex guide_vertices[] = {
        {-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    };

    glGenVertexArrays(1, &self->guide_vao);
    glGenBuffers(1, &self->guide_vbo);
    configure_vertex_array(self->guide_vao, self->guide_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, self->guide_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(guide_vertices), guide_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void
destroy_gl(StlViewerGlArea *self)
{
    if (self->mesh_vbo != 0) glDeleteBuffers(1, &self->mesh_vbo);
    if (self->mesh_vao != 0) glDeleteVertexArrays(1, &self->mesh_vao);
    if (self->edge_vbo != 0) glDeleteBuffers(1, &self->edge_vbo);
    if (self->edge_vao != 0) glDeleteVertexArrays(1, &self->edge_vao);
    if (self->measure_vbo != 0) glDeleteBuffers(1, &self->measure_vbo);
    if (self->measure_vao != 0) glDeleteVertexArrays(1, &self->measure_vao);
    if (self->guide_vbo != 0) glDeleteBuffers(1, &self->guide_vbo);
    if (self->guide_vao != 0) glDeleteVertexArrays(1, &self->guide_vao);
    if (self->program != 0) glDeleteProgram(self->program);

    self->mesh_vbo = 0;
    self->mesh_vao = 0;
    self->edge_vbo = 0;
    self->edge_vao = 0;
    self->measure_vbo = 0;
    self->measure_vao = 0;
    self->guide_vbo = 0;
    self->guide_vao = 0;
    self->program = 0;
    self->gl_ready = FALSE;
}

static void
build_matrices(StlViewerGlArea *self, int width, int height, float mvp[16], float normal[9])
{
    float projection[16];
    float view[16];
    float rot_x[16];
    float rot_y[16];
    float model[16];
    float view_model[16];

    float aspect = height > 0 ? (float)width / (float)height : 1.0f;
    aspect = MAX(aspect, 0.01f);

    mat4_perspective(projection, 42.0f * PI_F / 180.0f, aspect, 0.05f, 100.0f);
    mat4_translation(view, 0.0f, 0.0f, -3.35f * self->zoom);
    mat4_rotation_x(rot_x, self->pitch);
    mat4_rotation_y(rot_y, self->yaw);
    mat4_multiply(model, rot_y, rot_x);
    mat4_multiply(view_model, view, model);
    mat4_multiply(mvp, projection, view_model);
    normal_matrix_from_model(normal, model);
}

static void
draw_guides(StlViewerGlArea *self)
{
    glBindVertexArray(self->guide_vao);
    glLineWidth(2.0f);

    glUniform3f(self->color_location, 0.95f, 0.32f, 0.28f);
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(self->color_location, 0.34f, 0.70f, 0.38f);
    glDrawArrays(GL_LINES, 2, 2);

    glUniform3f(self->color_location, 0.35f, 0.55f, 0.95f);
    glDrawArrays(GL_LINES, 4, 2);

    glBindVertexArray(0);
}

static gboolean
on_render(GtkGLArea *area, GdkGLContext *context, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);
    int width = gtk_widget_get_width(GTK_WIDGET(area));
    int height = gtk_widget_get_height(GTK_WIDGET(area));
    float mvp[16];
    float normal[9];

    (void)context;

    if (!self->gl_ready)
        return TRUE;

    if (self->needs_upload)
        upload_mesh(self);
    if (self->needs_edge_upload)
        upload_edges(self);
    if (self->needs_measure_upload)
        upload_measurement(self);

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(self->background_color[0],
                 self->background_color[1],
                 self->background_color[2],
                 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    build_matrices(self, width, height, mvp, normal);
    glUseProgram(self->program);
    glUniformMatrix4fv(self->mvp_location, 1, GL_FALSE, mvp);
    glUniformMatrix3fv(self->normal_location, 1, GL_FALSE, normal);
    glUniform3f(self->light_direction_location,
                self->light_direction[0],
                self->light_direction[1],
                self->light_direction[2]);
    glUniform1f(self->ambient_location, self->ambient);
    glUniform1f(self->exposure_location, self->exposure);

    if (self->vertices->len == 0) {
        glUniform1i(self->shading_mode_location, SHADING_UNLIT);
        draw_guides(self);
        return TRUE;
    }

    glUniform1i(self->shading_mode_location, self->shading_mode);
    glUniform3f(self->color_location,
                self->material_color[0],
                self->material_color[1],
                self->material_color[2]);
    glBindVertexArray(self->mesh_vao);
    if (self->show_edges && self->edge_vertices->len > 0) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f);
    }
    glDrawArrays(GL_TRIANGLES, 0, self->vertices->len);
    if (self->show_edges && self->edge_vertices->len > 0)
        glDisable(GL_POLYGON_OFFSET_FILL);
    glBindVertexArray(0);

    if (self->show_edges && self->edge_vertices->len > 0) {
        glUniform1i(self->shading_mode_location, SHADING_UNLIT);
        glUniform3f(self->color_location, 0.02f, 0.025f, 0.03f);
        glBindVertexArray(self->edge_vao);
        glDrawArrays(GL_LINES, 0, self->edge_vertices->len);
        glBindVertexArray(0);
    }

    if (self->measure_vertices->len > 0) {
        glDisable(GL_DEPTH_TEST);
        glUniform1i(self->shading_mode_location, SHADING_UNLIT);
        glUniform3f(self->color_location, 1.0f, 0.82f, 0.18f);
        glBindVertexArray(self->measure_vao);
        glLineWidth(3.0f);
        glDrawArrays(GL_LINES, 0, self->measure_vertices->len);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }

    return TRUE;
}

static void
on_realize(GtkGLArea *area, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) != NULL)
        return;

    self->program = create_program(!epoxy_is_desktop_gl());
    if (self->program == 0) {
        set_status(self, "Unable to create OpenGL shader program");
        return;
    }

    self->mvp_location = glGetUniformLocation(self->program, "u_mvp");
    self->normal_location = glGetUniformLocation(self->program, "u_normal_matrix");
    self->color_location = glGetUniformLocation(self->program, "u_color");
    self->light_direction_location = glGetUniformLocation(self->program, "u_light_dir");
    self->ambient_location = glGetUniformLocation(self->program, "u_ambient");
    self->exposure_location = glGetUniformLocation(self->program, "u_exposure");
    self->shading_mode_location = glGetUniformLocation(self->program, "u_shading_mode");

    glGenVertexArrays(1, &self->mesh_vao);
    glGenBuffers(1, &self->mesh_vbo);
    configure_vertex_array(self->mesh_vao, self->mesh_vbo);

    glGenVertexArrays(1, &self->edge_vao);
    glGenBuffers(1, &self->edge_vbo);
    configure_vertex_array(self->edge_vao, self->edge_vbo);

    glGenVertexArrays(1, &self->measure_vao);
    glGenBuffers(1, &self->measure_vbo);
    configure_vertex_array(self->measure_vao, self->measure_vbo);

    create_guide_geometry(self);

    self->gl_ready = TRUE;
    self->needs_upload = TRUE;
    self->needs_edge_upload = TRUE;
    self->needs_measure_upload = TRUE;
}

static void
on_unrealize(GtkGLArea *area, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);

    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area) == NULL)
        destroy_gl(self);
}

static gboolean
unproject_point(const float inv_mvp[16], float x, float y, float z, Vec3 *out)
{
    float in[4] = {x, y, z, 1.0f};
    float transformed[4];

    mat4_transform_vec4(inv_mvp, in, transformed);
    if (fabsf(transformed[3]) < FLOAT_EPSILON)
        return FALSE;

    out->x = transformed[0] / transformed[3];
    out->y = transformed[1] / transformed[3];
    out->z = transformed[2] / transformed[3];
    return TRUE;
}

static gboolean
ray_intersects_triangle(Vec3 origin,
                        Vec3 direction,
                        Vec3 a,
                        Vec3 b,
                        Vec3 c,
                        float *out_t)
{
    Vec3 edge1 = vec3_sub(b, a);
    Vec3 edge2 = vec3_sub(c, a);
    Vec3 h = vec3_cross(direction, edge2);
    float det = vec3_dot(edge1, h);

    if (fabsf(det) < FLOAT_EPSILON)
        return FALSE;

    float inv_det = 1.0f / det;
    Vec3 s = vec3_sub(origin, a);
    float u = inv_det * vec3_dot(s, h);
    if (u < 0.0f || u > 1.0f)
        return FALSE;

    Vec3 q = vec3_cross(s, edge1);
    float v = inv_det * vec3_dot(direction, q);
    if (v < 0.0f || u + v > 1.0f)
        return FALSE;

    float t = inv_det * vec3_dot(edge2, q);
    if (t <= FLOAT_EPSILON)
        return FALSE;

    *out_t = t;
    return TRUE;
}

static gboolean
pick_surface_point(StlViewerGlArea *self, double x, double y, Vec3 *out)
{
    int width = gtk_widget_get_width(GTK_WIDGET(self));
    int height = gtk_widget_get_height(GTK_WIDGET(self));
    float mvp[16];
    float normal[9];
    float inv_mvp[16];
    float ndc_x;
    float ndc_y;
    Vec3 near_point;
    Vec3 far_point;
    Vec3 direction;
    gboolean hit = FALSE;
    float best_t = G_MAXFLOAT;
    Vec3 best_point = {0.0f, 0.0f, 0.0f};

    if (self->vertices->len == 0 || width <= 0 || height <= 0)
        return FALSE;

    build_matrices(self, width, height, mvp, normal);
    if (!mat4_invert(mvp, inv_mvp))
        return FALSE;

    ndc_x = (float)((2.0 * x) / (double)width - 1.0);
    ndc_y = (float)(1.0 - (2.0 * y) / (double)height);

    if (!unproject_point(inv_mvp, ndc_x, ndc_y, -1.0f, &near_point) ||
        !unproject_point(inv_mvp, ndc_x, ndc_y, 1.0f, &far_point))
        return FALSE;

    direction = vec3_normalize(vec3_sub(far_point, near_point));

    for (guint i = 0; i + 2 < self->vertices->len; i += 3) {
        StlVertex *va = &g_array_index(self->vertices, StlVertex, i);
        StlVertex *vb = &g_array_index(self->vertices, StlVertex, i + 1);
        StlVertex *vc = &g_array_index(self->vertices, StlVertex, i + 2);
        float t = 0.0f;

        if (ray_intersects_triangle(near_point,
                                    direction,
                                    vertex_position(va),
                                    vertex_position(vb),
                                    vertex_position(vc),
                                    &t) &&
            t < best_t) {
            best_t = t;
            best_point = vec3_add(near_point, vec3_scale(direction, t));
            hit = TRUE;
        }
    }

    if (!hit)
        return FALSE;

    *out = best_point;
    return TRUE;
}

static void
emit_measurement_changed(StlViewerGlArea *self)
{
    update_measurement_text(self);
    rebuild_measure_vertices(self);
    if (gtk_widget_get_realized(GTK_WIDGET(self))) {
        gtk_gl_area_make_current(GTK_GL_AREA(self));
        if (gtk_gl_area_get_error(GTK_GL_AREA(self)) == NULL)
            upload_measurement(self);
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
    g_signal_emit(self, signals[MEASUREMENT_CHANGED], 0);
}

static void
set_measurement_point(StlViewerGlArea *self, Vec3 point)
{
    if (!self->have_measure_a || self->have_measure_b) {
        self->measure_a = point;
        self->have_measure_a = TRUE;
        self->have_measure_b = FALSE;
        self->measurement_distance = 0.0f;
    } else {
        self->measure_b = point;
        self->have_measure_b = TRUE;
        self->measurement_distance = vec3_length(vec3_sub(self->measure_b, self->measure_a)) *
            self->model_unit_scale;
    }

    emit_measurement_changed(self);
}

static void
on_drag_begin(GtkGestureDrag *gesture, double start_x, double start_y, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);

    (void)gesture;
    (void)start_x;
    (void)start_y;

    if (self->measure_mode)
        return;

    self->drag_start_pitch = self->pitch;
    self->drag_start_yaw = self->yaw;
}

static void
on_drag_update(GtkGestureDrag *gesture, double offset_x, double offset_y, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);

    (void)gesture;

    if (self->measure_mode)
        return;

    self->yaw = self->drag_start_yaw + (float)offset_x * 0.010f;
    self->pitch = clampf(self->drag_start_pitch + (float)offset_y * 0.010f, -1.45f, 1.45f);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

static void
on_click_released(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);
    Vec3 point;

    (void)gesture;
    (void)n_press;

    if (!self->measure_mode)
        return;

    if (pick_surface_point(self, x, y, &point))
        set_measurement_point(self, point);
}

static gboolean
on_scroll(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(user_data);

    (void)controller;
    (void)dx;

    self->zoom = clampf(self->zoom * expf((float)dy * 0.10f), 0.35f, 4.5f);
    gtk_widget_queue_draw(GTK_WIDGET(self));
    return GDK_EVENT_STOP;
}

static void
stl_viewer_gl_area_finalize(GObject *object)
{
    StlViewerGlArea *self = STL_VIEWER_GL_AREA(object);

    g_clear_pointer(&self->vertices, g_array_unref);
    g_clear_pointer(&self->edge_vertices, g_array_unref);
    g_clear_pointer(&self->measure_vertices, g_array_unref);
    g_clear_pointer(&self->model_path, g_free);
    g_clear_pointer(&self->status, g_free);
    g_clear_pointer(&self->measurement_text, g_free);

    G_OBJECT_CLASS(stl_viewer_gl_area_parent_class)->finalize(object);
}

static void
stl_viewer_gl_area_class_init(StlViewerGlAreaClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = stl_viewer_gl_area_finalize;

    signals[MEASUREMENT_CHANGED] = g_signal_new("measurement-changed",
                                                G_TYPE_FROM_CLASS(klass),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                0);
}

static void
stl_viewer_gl_area_init(StlViewerGlArea *self)
{
    GtkGesture *drag;
    GtkGesture *click;
    GtkEventController *scroll;

    self->vertices = g_array_new(FALSE, FALSE, sizeof(StlVertex));
    self->edge_vertices = g_array_new(FALSE, FALSE, sizeof(StlVertex));
    self->measure_vertices = g_array_new(FALSE, FALSE, sizeof(StlVertex));
    self->pitch = DEFAULT_PITCH;
    self->yaw = DEFAULT_YAW;
    self->zoom = DEFAULT_ZOOM;
    self->material_color[0] = 0.72f;
    self->material_color[1] = 0.78f;
    self->material_color[2] = 0.86f;
    self->background_color[0] = 0.055f;
    self->background_color[1] = 0.060f;
    self->background_color[2] = 0.070f;
    self->ambient = 0.30f;
    self->exposure = 1.0f;
    self->shading_mode = SHADING_LIT;
    self->model_unit_scale = 1.0f;
    stl_viewer_gl_area_set_light_angles(self, 42.0, 45.0);
    set_status(self, "Open an STL file");
    update_measurement_text(self);

    gtk_gl_area_set_required_version(GTK_GL_AREA(self), 3, 2);
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(self), TRUE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(self), TRUE);
    gtk_widget_set_focusable(GTK_WIDGET(self), TRUE);

    g_signal_connect(self, "realize", G_CALLBACK(on_realize), self);
    g_signal_connect(self, "unrealize", G_CALLBACK(on_unrealize), self);
    g_signal_connect(self, "render", G_CALLBACK(on_render), self);

    drag = gtk_gesture_drag_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(drag), 0);
    g_signal_connect(drag, "drag-begin", G_CALLBACK(on_drag_begin), self);
    g_signal_connect(drag, "drag-update", G_CALLBACK(on_drag_update), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(drag));

    click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 1);
    g_signal_connect(click, "released", G_CALLBACK(on_click_released), self);
    gtk_widget_add_controller(GTK_WIDGET(self), GTK_EVENT_CONTROLLER(click));

    scroll = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(scroll, "scroll", G_CALLBACK(on_scroll), self);
    gtk_widget_add_controller(GTK_WIDGET(self), scroll);
}

StlViewerGlArea *
stl_viewer_gl_area_new(void)
{
    return g_object_new(STL_VIEWER_TYPE_GL_AREA, NULL);
}

gboolean
stl_viewer_gl_area_load_file(StlViewerGlArea *self, const char *path)
{
    GArray *loaded_vertices = NULL;
    guint loaded_triangles = 0;
    float loaded_scale = 1.0f;
    float loaded_width = 0.0f;
    float loaded_height = 0.0f;
    float loaded_depth = 0.0f;
    char *error_message = NULL;

    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), FALSE);
    g_return_val_if_fail(path != NULL, FALSE);

    if (!load_stl_vertices(path,
                           &loaded_vertices,
                           &loaded_triangles,
                           &loaded_scale,
                           &loaded_width,
                           &loaded_height,
                           &loaded_depth,
                           &error_message)) {
        set_status(self, error_message != NULL ? error_message : "Unable to load STL file");
        g_free(error_message);
        return FALSE;
    }

    g_array_unref(self->vertices);
    self->vertices = loaded_vertices;
    self->triangle_count = loaded_triangles;
    self->model_unit_scale = loaded_scale;
    self->bounds_width = loaded_width;
    self->bounds_height = loaded_height;
    self->bounds_depth = loaded_depth;
    self->have_measure_a = FALSE;
    self->have_measure_b = FALSE;
    self->measurement_distance = 0.0f;
    g_free(self->model_path);
    self->model_path = g_strdup(path);
    set_status(self, "Model loaded");
    update_measurement_text(self);
    rebuild_edge_vertices(self);
    rebuild_measure_vertices(self);

    self->needs_upload = TRUE;
    if (gtk_widget_get_realized(GTK_WIDGET(self))) {
        gtk_gl_area_make_current(GTK_GL_AREA(self));
        if (gtk_gl_area_get_error(GTK_GL_AREA(self)) == NULL) {
            upload_mesh(self);
            upload_edges(self);
            upload_measurement(self);
        }
    }
    gtk_widget_queue_draw(GTK_WIDGET(self));
    g_signal_emit(self, signals[MEASUREMENT_CHANGED], 0);

    return TRUE;
}

void
stl_viewer_gl_area_reset_view(StlViewerGlArea *self)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->pitch = DEFAULT_PITCH;
    self->yaw = DEFAULT_YAW;
    self->zoom = DEFAULT_ZOOM;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

guint
stl_viewer_gl_area_get_triangle_count(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), 0);
    return self->triangle_count;
}

const char *
stl_viewer_gl_area_get_status(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), "");
    return self->status != NULL ? self->status : "";
}

void
stl_viewer_gl_area_set_material_color(StlViewerGlArea *self, double red, double green, double blue)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->material_color[0] = clampf((float)red, 0.0f, 1.0f);
    self->material_color[1] = clampf((float)green, 0.0f, 1.0f);
    self->material_color[2] = clampf((float)blue, 0.0f, 1.0f);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_background_color(StlViewerGlArea *self, double red, double green, double blue)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->background_color[0] = clampf((float)red, 0.0f, 1.0f);
    self->background_color[1] = clampf((float)green, 0.0f, 1.0f);
    self->background_color[2] = clampf((float)blue, 0.0f, 1.0f);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_light_angles(StlViewerGlArea *self, double azimuth_degrees, double elevation_degrees)
{
    float azimuth = (float)azimuth_degrees * PI_F / 180.0f;
    float elevation = clampf((float)elevation_degrees, -85.0f, 85.0f) * PI_F / 180.0f;
    Vec3 direction = {
        cosf(elevation) * cosf(azimuth),
        sinf(elevation),
        cosf(elevation) * sinf(azimuth),
    };
    direction = vec3_normalize(direction);

    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->light_direction[0] = direction.x;
    self->light_direction[1] = direction.y;
    self->light_direction[2] = direction.z;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_lighting(StlViewerGlArea *self, double ambient, double exposure)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->ambient = clampf((float)ambient, 0.0f, 0.8f);
    self->exposure = clampf((float)exposure, 0.25f, 2.0f);
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_shading_mode(StlViewerGlArea *self, int mode)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    if (mode < SHADING_LIT || mode > SHADING_NORMALS)
        mode = SHADING_LIT;
    self->shading_mode = mode;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_show_edges(StlViewerGlArea *self, gboolean show_edges)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->show_edges = show_edges;
    gtk_widget_queue_draw(GTK_WIDGET(self));
}

void
stl_viewer_gl_area_set_measure_mode(StlViewerGlArea *self, gboolean measure_mode)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->measure_mode = measure_mode;
    emit_measurement_changed(self);
}

void
stl_viewer_gl_area_clear_measurement(StlViewerGlArea *self)
{
    g_return_if_fail(STL_VIEWER_IS_GL_AREA(self));

    self->have_measure_a = FALSE;
    self->have_measure_b = FALSE;
    self->measurement_distance = 0.0f;
    emit_measurement_changed(self);
}

double
stl_viewer_gl_area_get_measurement_distance(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), 0.0);
    return self->have_measure_b ? self->measurement_distance : 0.0;
}

const char *
stl_viewer_gl_area_get_measurement_text(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), "");
    return self->measurement_text != NULL ? self->measurement_text : "";
}

double
stl_viewer_gl_area_get_bounds_width(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), 0.0);
    return self->bounds_width;
}

double
stl_viewer_gl_area_get_bounds_height(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), 0.0);
    return self->bounds_height;
}

double
stl_viewer_gl_area_get_bounds_depth(StlViewerGlArea *self)
{
    g_return_val_if_fail(STL_VIEWER_IS_GL_AREA(self), 0.0);
    return self->bounds_depth;
}
