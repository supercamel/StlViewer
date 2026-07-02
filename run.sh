#!/bin/sh
set -eu

cd "$(dirname "$0")"

./native/build.sh

export GI_TYPELIB_PATH="$PWD/native/build${GI_TYPELIB_PATH:+:$GI_TYPELIB_PATH}"
export LD_LIBRARY_PATH="$PWD/native/build${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec sqgi main.nut "$@"
