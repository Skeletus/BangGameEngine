$input v_color0, v_uv0

#include <bgfx/bgfx_shader.sh>

// Sampler2D en slot 0
SAMPLER2D(s_texColor, 0);

void main()
{
    vec4 tex = texture2D(s_texColor, v_uv0);
    gl_FragColor = v_color0 * tex;
}
