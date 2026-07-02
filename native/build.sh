#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$script_dir"

build_dir="${BUILD_DIR:-build}"
meson_options="${MESON_OPTIONS:-}"

if [ -n "${MESON_CROSS_FILE:-}" ]; then
    meson setup "$build_dir" --wipe --cross-file "$MESON_CROSS_FILE" $meson_options || \
        meson setup "$build_dir" --cross-file "$MESON_CROSS_FILE" $meson_options
else
    meson setup "$build_dir" --wipe $meson_options || meson setup "$build_dir" $meson_options
fi

meson compile -C "$build_dir"
