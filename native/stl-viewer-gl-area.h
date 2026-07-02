#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define STL_VIEWER_TYPE_GL_AREA (stl_viewer_gl_area_get_type())

G_DECLARE_FINAL_TYPE(StlViewerGlArea, stl_viewer_gl_area, STL_VIEWER, GL_AREA, GtkGLArea)

/**
 * stl_viewer_gl_area_new:
 *
 * Creates a GTK widget that loads and displays STL meshes.
 *
 * Returns: (transfer full): a new #StlViewerGlArea
 */
StlViewerGlArea *stl_viewer_gl_area_new(void);

/**
 * stl_viewer_gl_area_load_file:
 * @self: a #StlViewerGlArea
 * @path: path to an ASCII or binary STL file
 *
 * Loads @path into the viewer.
 *
 * Returns: %TRUE when the file was parsed and uploaded
 */
gboolean stl_viewer_gl_area_load_file(StlViewerGlArea *self, const char *path);

/**
 * stl_viewer_gl_area_reset_view:
 * @self: a #StlViewerGlArea
 *
 * Restores the default camera rotation and zoom.
 */
void stl_viewer_gl_area_reset_view(StlViewerGlArea *self);

guint stl_viewer_gl_area_get_triangle_count(StlViewerGlArea *self);

/**
 * stl_viewer_gl_area_get_status:
 * @self: a #StlViewerGlArea
 *
 * Returns the latest load or view status.
 *
 * Returns: (transfer none): current status text
 */
const char *stl_viewer_gl_area_get_status(StlViewerGlArea *self);

void stl_viewer_gl_area_set_material_color(StlViewerGlArea *self,
                                           double red,
                                           double green,
                                           double blue);

void stl_viewer_gl_area_set_background_color(StlViewerGlArea *self,
                                             double red,
                                             double green,
                                             double blue);

void stl_viewer_gl_area_set_light_angles(StlViewerGlArea *self,
                                         double azimuth_degrees,
                                         double elevation_degrees);

void stl_viewer_gl_area_set_lighting(StlViewerGlArea *self,
                                     double ambient,
                                     double exposure);

void stl_viewer_gl_area_set_shading_mode(StlViewerGlArea *self, int mode);

void stl_viewer_gl_area_set_show_edges(StlViewerGlArea *self, gboolean show_edges);

void stl_viewer_gl_area_set_measure_mode(StlViewerGlArea *self, gboolean measure_mode);

void stl_viewer_gl_area_clear_measurement(StlViewerGlArea *self);

double stl_viewer_gl_area_get_measurement_distance(StlViewerGlArea *self);

/**
 * stl_viewer_gl_area_get_measurement_text:
 * @self: a #StlViewerGlArea
 *
 * Returns a human-readable summary of the current measurement state.
 *
 * Returns: (transfer none): current measurement text
 */
const char *stl_viewer_gl_area_get_measurement_text(StlViewerGlArea *self);

double stl_viewer_gl_area_get_bounds_width(StlViewerGlArea *self);
double stl_viewer_gl_area_get_bounds_height(StlViewerGlArea *self);
double stl_viewer_gl_area_get_bounds_depth(StlViewerGlArea *self);

G_END_DECLS
