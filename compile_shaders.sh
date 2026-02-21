#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHADERS_DIR="$SCRIPT_DIR/shaders"

echo "Compiling shaders with SDL_shadercross..."

shadercross "$SHADERS_DIR/base.vert.hlsl" -o "$SHADERS_DIR/base.vert.spv" \
    && echo "  base.vert.hlsl -> base.vert.spv"

shadercross "$SHADERS_DIR/base.frag.hlsl" -o "$SHADERS_DIR/base.frag.spv" \
    && echo "  base.frag.hlsl -> base.frag.spv"

shadercross "$SHADERS_DIR/skybox.vert.hlsl" -o "$SHADERS_DIR/skybox.vert.spv" \
    && echo "  skybox.vert.hlsl -> skybox.vert.spv"

shadercross "$SHADERS_DIR/skybox.frag.hlsl" -o "$SHADERS_DIR/skybox.frag.spv" \
    && echo "  skybox.frag.hlsl -> skybox.frag.spv"

echo "Done!"
