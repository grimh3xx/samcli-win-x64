#!/bin/sh
# Compila o samcli no Linux (gcc ou clang).
set -e
CC=${CC:-cc}
$CC -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -o samcli \
    src/main.c src/steam.c src/ui.c src/platform.c -ldl
echo "ok: ./samcli"
