local GLib = import("GLib")
local Gio = import("Gio")
local Gtk = import("Gtk", "4.0")
local StlViewer = import("StlViewer", "1.0")

local AppConfig = {
    id = "io.github.sam.StlViewer",
    title = "STL Viewer",
}

local Presets = {
    materials = [
        { name = "Steel", color = [0.72, 0.78, 0.86] },
        { name = "Clay", color = [0.78, 0.56, 0.42] },
        { name = "Graphite", color = [0.28, 0.30, 0.34] },
        { name = "Resin", color = [0.28, 0.72, 0.64] },
        { name = "Coral", color = [0.94, 0.40, 0.34] },
    ],
    backgrounds = [
        { name = "Charcoal", color = [0.055, 0.060, 0.070] },
        { name = "Paper", color = [0.92, 0.91, 0.88] },
        { name = "Ink", color = [0.01, 0.014, 0.018] },
    ],
    shading_modes = ["Lit", "Unlit", "Normals"],
}

local Formatters = {
    number = function(value) {
        return format("%.4g", value)
    },

    display_name = function(path) {
        if (path == null) return AppConfig.title

        local file = Gio.File.new_for_path(path)
        local file_name = file.get_basename()
        return file_name != null ? file_name : path
    },
}

class CommandLineOptions {
    hard_timeout = 0
    initial_path = null

    constructor(args) {
        foreach (arg in args) {
            if (arg.find("--timeout=") == 0) {
                this.hard_timeout = arg.slice(10).tointeger()
            } else if (arg == "--sample") {
                this.initial_path = this.sample_path()
            } else if (arg.len() > 0 && arg.slice(0, 1) != "-") {
                this.initial_path = arg
            }
        }
    }

    function sample_path() {
        local resources = GLib.getenv("SQGI_APP_RESOURCES")
        if (resources != null) {
            return GLib.build_filenamev([resources, "examples", "cube.stl"])
        }
        return GLib.build_filenamev(["examples", "cube.stl"])
    }
}

class PresetGroup {
    items = null

    constructor(items) {
        this.items = items
    }

    function names() {
        local values = []
        foreach (item in this.items) {
            values.append(item.name)
        }
        return values
    }

    function color(index) {
        if (index < 0 || index >= this.items.len()) index = 0
        return this.items[index].color
    }
}

class WidgetRegistry {
    items = null

    constructor() {
        this.items = {}
    }

    function remember(name, widget) {
        this.items[name] <- widget
        return widget
    }

    function has(name) {
        return name in this.items
    }

    function get(name) {
        return this.items[name]
    }
}

class ViewerState {
    loaded = false
    model_name = null

    function mark_loaded(name) {
        this.loaded = true
        this.model_name = name
    }

    function clear() {
        this.loaded = false
        this.model_name = null
    }
}

class UiBuilder {
    function icon_button(icon_name, tooltip) {
        local button = Gtk.Button.new_from_icon_name(icon_name)
        button.set_tooltip_text(tooltip)
        return button
    }

    function section_title(text) {
        local label = Gtk.Label.new(text)
        label.set_xalign(0.0)
        label.add_css_class("heading")
        return label
    }

    function row(label_text, child) {
        local box = Gtk.Box.new(Gtk.Orientation.horizontal, 8)
        local label = Gtk.Label.new(label_text)
        label.set_xalign(0.0)
        label.set_hexpand(true)
        box.append(label)
        box.append(child)
        return box
    }

    function dropdown(names) {
        return Gtk.DropDown.new(Gtk.StringList.new(names), null)
    }

    function slider(value, min_value, max_value, step) {
        local adj = Gtk.Adjustment.new(value, min_value, max_value, step, step * 10.0, 0.0)
        local scale = Gtk.Scale.new(Gtk.Orientation.horizontal, adj)
        scale.set_draw_value(true)
        scale.set_hexpand(true)
        return { widget = scale, adjustment = adj }
    }

    function separator(box) {
        box.append(Gtk.Separator.new(Gtk.Orientation.horizontal))
    }

    function install_css(display) {
        local css =
            ".floating-options {" +
            "  background: rgba(30, 33, 37, 0.92);" +
            "  color: rgba(246, 247, 249, 0.96);" +
            "  border: 1px solid rgba(255, 255, 255, 0.16);" +
            "  border-radius: 14px;" +
            "  box-shadow: 0 14px 36px rgba(0, 0, 0, 0.36);" +
            "}" +
            ".floating-options .dim-label {" +
            "  color: rgba(224, 228, 235, 0.76);" +
            "}" +
            ".floating-options separator {" +
            "  background: rgba(255, 255, 255, 0.14);" +
            "}" +
            ".measure-footer {" +
            "  border-top: 1px solid rgba(255, 255, 255, 0.14);" +
            "}" +
            ".floating-options scrolledwindow {" +
            "  background: transparent;" +
            "  border: none;" +
            "}" +
            ".floating-options viewport {" +
            "  background: transparent;" +
            "}"

        local provider = Gtk.CssProvider.new()
        provider.load_from_data(css, css.len())
        Gtk.StyleContext.add_provider_for_display(
            display,
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )
    }
}

class ViewerApp {
    options = null
    app = null
    state = null
    widgets = null
    ui = null
    material_presets = null
    background_presets = null

    constructor(options) {
        this.options = options
        this.app = Gtk.Application.new(AppConfig.id, Gio.ApplicationFlags.flags_none)
        this.state = ViewerState()
        this.widgets = WidgetRegistry()
        this.ui = UiBuilder()
        this.material_presets = PresetGroup(Presets.materials)
        this.background_presets = PresetGroup(Presets.backgrounds)
    }

    function run() {
        local self = this
        this.app.connect("activate", function() {
            self.build_window()
        })

        local status = this.app.run(0, null)
        print("Application exited with status " + status + "\n")
        return status
    }

    function widget(name) {
        return this.widgets.get(name)
    }

    function viewer() {
        return this.widget("viewer")
    }

    function set_status(text) {
        if (this.widgets.has("status")) this.widget("status").set_text(text)
    }

    function set_subtitle(text) {
        if (this.widgets.has("subtitle")) this.widget("subtitle").set_text(text)
    }

    function unit_scale_value() {
        return this.widgets.has("unit_scale") ? this.widget("unit_scale").get_value() : 1.0
    }

    function build_window() {
        local win = Gtk.ApplicationWindow.new(this.app)
        this.widgets.remember("win", win)
        win.set_title(AppConfig.title)
        win.set_default_size(1120, 760)
        win.set_titlebar(this.build_header())

        local root = Gtk.Box.new(Gtk.Orientation.vertical, 0)
        win.set_child(root)
        root.append(this.build_viewer_overlay())
        root.append(this.build_status_bar())

        this.apply_initial_view_settings()
        this.ui.install_css(win.get_display())
        win.present()

        if (this.options.initial_path != null) this.load_model(this.options.initial_path)
        if (this.options.hard_timeout > 0) this.install_timeout()
    }

    function build_header() {
        local self = this
        local header = Gtk.HeaderBar.new()

        local open_button = this.ui.icon_button("document-open-symbolic", "Open STL")
        open_button.connect("clicked", function() {
            self.open_dialog()
        })
        header.pack_start(open_button)

        local reset_button = this.ui.icon_button("view-refresh-symbolic", "Reset view")
        reset_button.connect("clicked", function() {
            self.viewer().reset_view()
            self.set_status(self.viewer().get_status())
        })
        header.pack_end(reset_button)

        local title_box = Gtk.Box.new(Gtk.Orientation.vertical, 0)
        local title = Gtk.Label.new(AppConfig.title)
        title.add_css_class("heading")
        local subtitle = Gtk.Label.new("No model loaded")
        subtitle.add_css_class("dim-label")
        title_box.append(title)
        title_box.append(subtitle)
        this.widgets.remember("subtitle", subtitle)
        header.set_title_widget(title_box)

        return header
    }

    function build_viewer_overlay() {
        local self = this
        local overlay = Gtk.Overlay.new()
        overlay.set_hexpand(true)
        overlay.set_vexpand(true)

        local viewer = StlViewer.GlArea.new()
        viewer.set_hexpand(true)
        viewer.set_vexpand(true)
        viewer.connect("measurement-changed", function() {
            self.update_measurement()
        })
        this.widgets.remember("viewer", viewer)
        overlay.set_child(viewer)

        local option_shell = this.build_options_shell()
        overlay.add_overlay(option_shell)
        overlay.set_measure_overlay(option_shell, false)
        overlay.set_clip_overlay(option_shell, true)

        return overlay
    }

    function build_options_shell() {
        local option_shell = Gtk.Box.new(Gtk.Orientation.vertical, 0)
        option_shell.add_css_class("floating-options")
        option_shell.set_halign(Gtk.Align.end)
        option_shell.set_valign(Gtk.Align.fill)
        option_shell.set_margin_top(16)
        option_shell.set_margin_bottom(16)
        option_shell.set_margin_start(16)
        option_shell.set_margin_end(16)
        option_shell.set_size_request(320, -1)

        local option_scroll = Gtk.ScrolledWindow.new()
        option_scroll.set_child(this.build_options_panel())
        option_scroll.set_hexpand(true)
        option_scroll.set_vexpand(true)
        option_shell.append(option_scroll)
        option_shell.append(this.build_measure_panel())

        return option_shell
    }

    function build_options_panel() {
        local self = this
        local panel = Gtk.Box.new(Gtk.Orientation.vertical, 10)
        panel.set_margin_top(12)
        panel.set_margin_bottom(12)
        panel.set_margin_start(12)
        panel.set_margin_end(12)
        panel.set_size_request(280, -1)

        panel.append(this.ui.section_title("Material"))

        local material = this.ui.dropdown(this.material_presets.names())
        material.connect("notify::selected", function(pspec) {
            self.apply_material(material.get_selected())
        })
        panel.append(this.ui.row("Color", material))

        local shading = this.ui.dropdown(Presets.shading_modes)
        shading.connect("notify::selected", function(pspec) {
            self.viewer().set_shading_mode(shading.get_selected())
        })
        panel.append(this.ui.row("Shading", shading))

        local edges = Gtk.CheckButton.new_with_label("Edges")
        edges.connect("toggled", function() {
            self.viewer().set_show_edges(edges.get_active())
        })
        panel.append(edges)

        this.ui.separator(panel)
        panel.append(this.ui.section_title("Lighting"))

        local azimuth_pair = this.ui.slider(42.0, -180.0, 180.0, 1.0)
        this.widgets.remember("azimuth", azimuth_pair.adjustment)
        azimuth_pair.adjustment.connect("value-changed", function() {
            self.apply_lighting()
        })
        panel.append(this.ui.row("Azimuth", azimuth_pair.widget))

        local elevation_pair = this.ui.slider(45.0, -20.0, 85.0, 1.0)
        this.widgets.remember("elevation", elevation_pair.adjustment)
        elevation_pair.adjustment.connect("value-changed", function() {
            self.apply_lighting()
        })
        panel.append(this.ui.row("Elevation", elevation_pair.widget))

        local ambient_pair = this.ui.slider(0.30, 0.0, 0.8, 0.01)
        this.widgets.remember("ambient", ambient_pair.adjustment)
        ambient_pair.adjustment.connect("value-changed", function() {
            self.apply_lighting()
        })
        panel.append(this.ui.row("Ambient", ambient_pair.widget))

        local exposure_pair = this.ui.slider(1.0, 0.25, 2.0, 0.01)
        this.widgets.remember("exposure", exposure_pair.adjustment)
        exposure_pair.adjustment.connect("value-changed", function() {
            self.apply_lighting()
        })
        panel.append(this.ui.row("Exposure", exposure_pair.widget))

        this.ui.separator(panel)
        panel.append(this.ui.section_title("Canvas"))

        local background = this.ui.dropdown(this.background_presets.names())
        background.connect("notify::selected", function(pspec) {
            self.apply_background(background.get_selected())
        })
        panel.append(this.ui.row("Background", background))

        return panel
    }

    function build_measure_panel() {
        local self = this
        local panel = Gtk.Box.new(Gtk.Orientation.vertical, 10)
        panel.add_css_class("measure-footer")
        panel.set_margin_top(12)
        panel.set_margin_bottom(12)
        panel.set_margin_start(12)
        panel.set_margin_end(12)

        panel.append(this.ui.section_title("Measure"))

        local measure_row = Gtk.Box.new(Gtk.Orientation.horizontal, 8)
        local measure = Gtk.CheckButton.new_with_label("Measure")
        measure.connect("toggled", function() {
            self.viewer().set_measure_mode(measure.get_active())
            self.update_measurement()
        })
        measure_row.append(measure)

        local clear = this.ui.icon_button("edit-clear-symbolic", "Clear measurement")
        clear.connect("clicked", function() {
            self.viewer().clear_measurement()
            self.update_measurement()
        })
        measure_row.append(clear)
        panel.append(measure_row)
        this.widgets.remember("measure_toggle", measure)

        local measurement = Gtk.Label.new("Measure: inactive")
        measurement.set_xalign(0.0)
        measurement.add_css_class("dim-label")
        this.widgets.remember("measurement", measurement)
        panel.append(measurement)

        local unit_adj = Gtk.Adjustment.new(1.0, 0.0001, 1000000.0, 0.1, 1.0, 0.0)
        local unit_scale = Gtk.SpinButton.new(unit_adj, 0.1, 4)
        unit_scale.connect("value-changed", function() {
            self.update_measurement()
            self.update_model_info()
        })
        this.widgets.remember("unit_scale", unit_scale)
        panel.append(this.ui.row("Unit scale", unit_scale))

        local dimensions = Gtk.Label.new("No model")
        dimensions.set_xalign(0.0)
        dimensions.add_css_class("dim-label")
        this.widgets.remember("dimensions", dimensions)
        panel.append(dimensions)

        return panel
    }

    function build_status_bar() {
        local status_bar = Gtk.Box.new(Gtk.Orientation.horizontal, 8)
        status_bar.set_margin_top(8)
        status_bar.set_margin_bottom(8)
        status_bar.set_margin_start(10)
        status_bar.set_margin_end(10)

        local status = Gtk.Label.new("Open an STL file")
        status.set_xalign(0.0)
        status.set_hexpand(true)
        this.widgets.remember("status", status)
        status_bar.append(status)

        return status_bar
    }

    function apply_initial_view_settings() {
        this.apply_material(0)
        this.apply_background(0)
        this.apply_lighting()
        this.update_model_info()
        this.update_measurement()
    }

    function apply_material(index) {
        local color = this.material_presets.color(index)
        this.viewer().set_material_color(color[0], color[1], color[2])
    }

    function apply_background(index) {
        local color = this.background_presets.color(index)
        this.viewer().set_background_color(color[0], color[1], color[2])
    }

    function apply_lighting() {
        this.viewer().set_light_angles(
            this.widget("azimuth").get_value(),
            this.widget("elevation").get_value()
        )
        this.viewer().set_lighting(
            this.widget("ambient").get_value(),
            this.widget("exposure").get_value()
        )
    }

    function measurement_text() {
        local point_count = this.viewer().get_measurement_point_count()
        if (point_count >= 2) {
            local distance = this.viewer().get_measurement_distance() * this.unit_scale_value()
            return Formatters.number(distance) + " units"
        }

        if (point_count == 1) return "Measure: click the second point"

        if (this.widgets.has("measure_toggle") && this.widget("measure_toggle").get_active()) {
            return "Measure: click the first point"
        }

        return "Measure: inactive"
    }

    function update_measurement() {
        if (!this.widgets.has("measurement")) return
        this.widget("measurement").set_text(this.measurement_text())
    }

    function update_model_info() {
        if (!this.widgets.has("dimensions")) return

        if (!this.state.loaded) {
            this.widget("dimensions").set_text("No model")
            return
        }

        local scale = this.unit_scale_value()
        local width = this.viewer().get_bounds_width() * scale
        local height = this.viewer().get_bounds_height() * scale
        local depth = this.viewer().get_bounds_depth() * scale
        this.widget("dimensions").set_text(
            Formatters.number(width) + " x " +
            Formatters.number(height) + " x " +
            Formatters.number(depth) + " units"
        )
    }

    function load_model(path) {
        if (path == null || path.len() == 0) return

        if (this.viewer().load_file(path)) {
            local triangles = this.viewer().get_triangle_count()
            local name = Formatters.display_name(path)
            this.state.mark_loaded(name)
            this.widget("win").set_title(name + " - " + AppConfig.title)
            this.set_subtitle(name)
            this.set_status(format("%d triangles", triangles))
        } else {
            local message = this.viewer().get_status()
            this.state.clear()
            this.set_status(message)
            this.set_subtitle("No model loaded")
            print("load failed: " + message + "\n")
        }

        this.update_model_info()
        this.update_measurement()
    }

    function open_dialog() {
        local self = this
        local chooser = Gtk.FileChooserNative.new(
            "Open STL",
            this.widget("win"),
            Gtk.FileChooserAction.open,
            "_Open",
            "_Cancel"
        )

        local stl_filter = Gtk.FileFilter.new()
        stl_filter.set_name("STL models")
        stl_filter.add_pattern("*.stl")
        stl_filter.add_pattern("*.STL")
        chooser.add_filter(stl_filter)

        local all_filter = Gtk.FileFilter.new()
        all_filter.set_name("All files")
        all_filter.add_pattern("*")
        chooser.add_filter(all_filter)

        chooser.connect("response", function(response) {
            if (response == Gtk.ResponseType.accept) {
                local file = chooser.get_file()
                if (file != null) {
                    local path = file.get_path()
                    if (path != null) self.load_model(path)
                }
            }
            chooser.destroy()
        })

        chooser.show()
    }

    function install_timeout() {
        local self = this
        sqgi.timeout_add(this.options.hard_timeout * 1000, function() {
            self.app.quit()
            return false
        })
    }
}

local options = CommandLineOptions(vargv)
local controller = ViewerApp(options)
return controller.run()
