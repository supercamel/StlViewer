# StlViewer

A small SQGI/GTK4 STL viewer with a native GObject widget backed by
`GtkGLArea`.

## Run

```sh
./run.sh --sample
./run.sh path/to/model.stl
```

`run.sh` builds the native introspection module and runs `main.nut` with the
local library and typelib paths.

Use the open button to choose an STL file, drag in the viewport to rotate, use
the mouse wheel or touchpad scroll to zoom, and use the reset button to restore
the default view.

The side panel includes:

- point-to-point measurement on the mesh surface
- unit scale and model dimensions
- material color, shading mode, and edge overlay
- light direction, ambient light, exposure, and background presets

## Package Smoke Test

```sh
sqgipkg --doctor
xvfb-run -a sqgipkg --smoke-test "--sample --timeout=2"
```

The manifest declares the GTK/OpenGL widget as a native SQGI project, includes a
desktop icon, and defines Linux x86_64, Linux aarch64, and Windows targets:

```sh
sqgipkg --target appimage --appimage-arch x86_64
sqgipkg --target appimage --appimage-arch aarch64
sqgipkg --target win-nsis
```

The package manifest includes `examples/cube.stl` and
`images/io.github.sam.StlViewer.png` as app resources so the sample path and app
icon also work from packaged builds.
