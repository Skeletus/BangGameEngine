$input a_position, a_normal, a_color0, a_texcoord0
$output v_worldPos, v_worldNormal, v_color0, v_uv0

#include <bgfx/bgfx_shader.sh>

uniform mat4 u_normalMtx;

void main()
{
    gl_Position   = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_worldPos    = mul(vec4(a_position, 1.0), u_model[0]).xyz;
    v_worldNormal = normalize(mul(vec4(a_normal, 0.0), u_normalMtx).xyz);

    v_color0 = a_color0;
    v_uv0    = a_texcoord0;
}