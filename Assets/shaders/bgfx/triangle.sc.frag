// Simple bgfx fragment shader for triangle smoke test
// This is a minimal source; use `scripts/compile_bgfx_shaders.ps1` to compile for target backends.

$input v_color0

void main() {
    vec4 col = vec4(v_color0, 1.0);
    gl_FragColor = col;
}
