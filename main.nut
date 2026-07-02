local GLib = import("GLib")
local Gio = import("Gio")
local Gtk = import("Gtk", "4.0")
local StlViewer = import("StlViewer", "1.0")

local app_id = "io.github.sam.StlViewer"
local hard_timeout = 0
local initial_path = null

foreach (arg in vargv) {
    if (arg.find("--timeout=") == 0) {
        hard_timeout = arg.slice(10).tointeger()
    } else if (arg == "--sample") {
        local resources = GLib.getenv("SQGI_APP_RESOURCES")
        if (resources != null) {
            initial_path = GLib.build_filenamev([resources, "examples", "cube.stl"])
        } else {
            initial_path = GLib.build_filenamev(["examples", "cube.stl"])
        }
    } else if (arg.len() > 0 && arg.slice(0, 1) != "-") {
        initial_path = arg
    }
}

local app = Gtk.Application.new(app_id, Gio.ApplicationFlags.flags_none)
local W = {}
local State = {
    loaded = false,
    model_name = null,
}

local material_presets = [
    { name = "Steel", color = [0.72, 0.78, 0.86] },
    { name = "Clay", color = [0.78, 0.56, 0.42] },
    { name = "Graphite", color = [0.28, 0.30, 0.34] },
    { name = "Resin", color = [0.28, 0.72, 0.64] },
    { name = "Coral", color = [0.94, 0.40, 0.34] },
]

local background_presets = [
    { name = "Charcoal", color = [0.055, 0.060, 0.070] },
    { name = "Paper", color = [0.92, 0.91, 0.88] },
    { name = "Ink", color = [0.01, 0.014, 0.018] },
]

function remember(name, value) {
    W[name] <- value
    return value
}

function display_name(path) {
    if (path == null) return "STL Viewer"
    local file = Gio.File.new_for_path(path)
    local file_name = file.get_basename()
    return file_name != null ? file_name : path
}

function format_number(value) {
    return format("%.4g", value)
}

function set_status(text) {
    if ("status" in W) W.status.set_text(text)
}

function set_subtitle(text) {
    if ("subtitle" in W) W.subtitle.set_text(text)
}

function apply_lighting() {
    W.viewer.set_light_angles(W.azimuth.get_value(), W.elevation.get_value())
    W.viewer.set_lighting(W.ambient.get_value(), W.exposure.get_value())
}

function update_measurement() {
    if (!("measurement" in W)) return

    local distance = W.viewer.get_measurement_distance()
    if (distance > 0.0) {
        W.measurement.set_text(format_number(distance * W.unit_scale.get_value()) + " units")
    } else {
        W.measurement.set_text(W.viewer.get_measurement_text())
    }
}

function update_model_info() {
    if (!("dimensions" in W)) return

    if (!State.loaded) {
        W.dimensions.set_text("No model")
        return
    }

    local scale = W.unit_scale.get_value()
    local width = W.viewer.get_bounds_width() * scale
    local height = W.viewer.get_bounds_height() * scale
    local depth = W.viewer.get_bounds_depth() * scale
    W.dimensions.set_text(format_number(width) + " x " +
                          format_number(height) + " x " +
                          format_number(depth) + " units")
}

function apply_material(index) {
    local preset = material_presets[index]
    W.viewer.set_material_color(preset.color[0], preset.color[1], preset.color[2])
}

function apply_background(index) {
    local preset = background_presets[index]
    W.viewer.set_background_color(preset.color[0], preset.color[1], preset.color[2])
}

function load_model(path) {
    if (path == null || path.len() == 0) return

    if (W.viewer.load_file(path)) {
        local triangles = W.viewer.get_triangle_count()
        State.loaded = true
        State.model_name = display_name(path)
        W.win.set_title(State.model_name + " - STL Viewer")
        set_subtitle(State.model_name)
        set_status(format("%d triangles", triangles))
        update_model_info()
        update_measurement()
    } else {
        local message = W.viewer.get_status()
        State.loaded = false
        State.model_name = null
        set_status(message)
        set_subtitle("No model loaded")
        update_model_info()
        update_measurement()
        print("load failed: " + message + "\n")
    }
}

function open_dialog() {
    local chooser = Gtk.FileChooserNative.new(
        "Open STL",
        W.win,
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
                if (path != null) load_model(path)
            }
        }
        chooser.destroy()
    })

    chooser.show()
}

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

function add_separator(box) {
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

function build_measure_panel() {
    local panel = Gtk.Box.new(Gtk.Orientation.vertical, 10)
    panel.add_css_class("measure-footer")
    panel.set_margin_top(12)
    panel.set_margin_bottom(12)
    panel.set_margin_start(12)
    panel.set_margin_end(12)

    panel.append(section_title("Measure"))

    local measure_row = Gtk.Box.new(Gtk.Orientation.horizontal, 8)
    local measure = Gtk.CheckButton.new_with_label("Measure")
    measure.connect("toggled", function() {
        W.viewer.set_measure_mode(measure.get_active())
        update_measurement()
    })
    measure_row.append(measure)

    local clear = icon_button("edit-clear-symbolic", "Clear measurement")
    clear.connect("clicked", function() {
        W.viewer.clear_measurement()
        update_measurement()
    })
    measure_row.append(clear)
    panel.append(measure_row)
    remember("measure_toggle", measure)

    local measurement = Gtk.Label.new("Measure: inactive")
    measurement.set_xalign(0.0)
    measurement.add_css_class("dim-label")
    remember("measurement", measurement)
    panel.append(measurement)

    local unit_adj = Gtk.Adjustment.new(1.0, 0.0001, 1000000.0, 0.1, 1.0, 0.0)
    local unit_scale = Gtk.SpinButton.new(unit_adj, 0.1, 4)
    unit_scale.connect("value-changed", function() {
        update_measurement()
        update_model_info()
    })
    remember("unit_scale", unit_scale)
    panel.append(row("Unit scale", unit_scale))

    local dimensions = Gtk.Label.new("No model")
    dimensions.set_xalign(0.0)
    dimensions.add_css_class("dim-label")
    remember("dimensions", dimensions)
    panel.append(dimensions)

    return panel
}

function build_options_panel() {
    local panel = Gtk.Box.new(Gtk.Orientation.vertical, 10)
    panel.set_margin_top(12)
    panel.set_margin_bottom(12)
    panel.set_margin_start(12)
    panel.set_margin_end(12)
    panel.set_size_request(280, -1)

    panel.append(section_title("Material"))

    local material = dropdown(["Steel", "Clay", "Graphite", "Resin", "Coral"])
    material.connect("notify::selected", function(pspec) {
        apply_material(material.get_selected())
    })
    panel.append(row("Color", material))

    local shading = dropdown(["Lit", "Unlit", "Normals"])
    shading.connect("notify::selected", function(pspec) {
        W.viewer.set_shading_mode(shading.get_selected())
    })
    panel.append(row("Shading", shading))

    local edges = Gtk.CheckButton.new_with_label("Edges")
    edges.connect("toggled", function() {
        W.viewer.set_show_edges(edges.get_active())
    })
    panel.append(edges)

    add_separator(panel)
    panel.append(section_title("Lighting"))

    local azimuth_pair = slider(42.0, -180.0, 180.0, 1.0)
    remember("azimuth", azimuth_pair.adjustment)
    azimuth_pair.adjustment.connect("value-changed", function() { apply_lighting() })
    panel.append(row("Azimuth", azimuth_pair.widget))

    local elevation_pair = slider(45.0, -20.0, 85.0, 1.0)
    remember("elevation", elevation_pair.adjustment)
    elevation_pair.adjustment.connect("value-changed", function() { apply_lighting() })
    panel.append(row("Elevation", elevation_pair.widget))

    local ambient_pair = slider(0.30, 0.0, 0.8, 0.01)
    remember("ambient", ambient_pair.adjustment)
    ambient_pair.adjustment.connect("value-changed", function() { apply_lighting() })
    panel.append(row("Ambient", ambient_pair.widget))

    local exposure_pair = slider(1.0, 0.25, 2.0, 0.01)
    remember("exposure", exposure_pair.adjustment)
    exposure_pair.adjustment.connect("value-changed", function() { apply_lighting() })
    panel.append(row("Exposure", exposure_pair.widget))

    add_separator(panel)
    panel.append(section_title("Canvas"))

    local background = dropdown(["Charcoal", "Paper", "Ink"])
    background.connect("notify::selected", function(pspec) {
        apply_background(background.get_selected())
    })
    panel.append(row("Background", background))

    return panel
}

function build_window() {
    local win = Gtk.ApplicationWindow.new(app)
    remember("win", win)
    win.set_title("STL Viewer")
    win.set_default_size(1120, 760)

    local header = Gtk.HeaderBar.new()

    local open_button = icon_button("document-open-symbolic", "Open STL")
    open_button.connect("clicked", function() { open_dialog() })
    header.pack_start(open_button)

    local reset_button = icon_button("view-refresh-symbolic", "Reset view")
    reset_button.connect("clicked", function() {
        W.viewer.reset_view()
        set_status(W.viewer.get_status())
    })
    header.pack_end(reset_button)

    local title_box = Gtk.Box.new(Gtk.Orientation.vertical, 0)
    local title = Gtk.Label.new("STL Viewer")
    title.add_css_class("heading")
    local subtitle = Gtk.Label.new("No model loaded")
    subtitle.add_css_class("dim-label")
    title_box.append(title)
    title_box.append(subtitle)
    remember("subtitle", subtitle)
    header.set_title_widget(title_box)
    win.set_titlebar(header)

    local root = Gtk.Box.new(Gtk.Orientation.vertical, 0)
    win.set_child(root)

    local overlay = Gtk.Overlay.new()
    overlay.set_hexpand(true)
    overlay.set_vexpand(true)
    root.append(overlay)

    local viewer = StlViewer.GlArea.new()
    viewer.set_hexpand(true)
    viewer.set_vexpand(true)
    viewer.connect("measurement-changed", function() { update_measurement() })
    remember("viewer", viewer)
    overlay.set_child(viewer)

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
    option_scroll.set_child(build_options_panel())
    option_scroll.set_hexpand(true)
    option_scroll.set_vexpand(true)
    option_shell.append(option_scroll)
    option_shell.append(build_measure_panel())

    overlay.add_overlay(option_shell)
    overlay.set_measure_overlay(option_shell, false)
    overlay.set_clip_overlay(option_shell, true)

    local status_bar = Gtk.Box.new(Gtk.Orientation.horizontal, 8)
    status_bar.set_margin_top(8)
    status_bar.set_margin_bottom(8)
    status_bar.set_margin_start(10)
    status_bar.set_margin_end(10)

    local status = Gtk.Label.new("Open an STL file")
    status.set_xalign(0.0)
    status.set_hexpand(true)
    remember("status", status)
    status_bar.append(status)
    root.append(status_bar)

    apply_material(0)
    apply_background(0)
    apply_lighting()
    update_model_info()
    update_measurement()

    install_css(win.get_display())
    win.present()

    if (initial_path != null) load_model(initial_path)
    if (hard_timeout > 0) {
        sqgi.timeout_add(hard_timeout * 1000, function() {
            app.quit()
            return false
        })
    }
}

app.connect("activate", function() {
    build_window()
})

local status = app.run(0, null)
print("Application exited with status " + status + "\n")
return status
