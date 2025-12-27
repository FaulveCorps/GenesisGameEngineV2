// Simple bgfx vertex shader for triangle smoke test
// This is a minimal source; use `scripts/compile_bgfx_shaders.ps1` to compile for target backends.

$input a_position, a_color0
$output v_color0

void main() {
    v_color0 = a_color0;
    // pass position through as clip-space position
    gl_Position = vec4(a_position, 1.0);
}
