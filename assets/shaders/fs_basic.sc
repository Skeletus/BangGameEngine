$input v_worldPos, v_worldNormal, v_color0, v_uv0

#include <bgfx/bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

uniform vec4 u_lightDir;
uniform vec4 u_lightColor;
uniform vec4 u_ambient;

uniform vec4 u_cameraPos;
uniform vec4 u_specParams;
uniform vec4 u_specColor;

uniform vec4 u_baseTint;
uniform vec4 u_uvScale;

void main()
{
    vec3 N = normalize(v_worldNormal);
    vec3 L = normalize(-u_lightDir.xyz);
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos);

    float diff = max(dot(N, L), 0.0);

    vec2 uv = v_uv0 * u_uvScale.xy;
    vec4 tex = texture2D(s_texColor, uv);

    vec3 base = tex.rgb * v_color0.rgb * u_baseTint.rgb;
    
    vec3 H = normalize(L + V);
    float s = pow(max(dot(N, H), 0.0), u_specParams.x) * u_specParams.y;

    vec3 lit = u_ambient.rgb + u_lightColor.rgb * diff;
    vec3 rgb = base * lit + u_specColor.rgb * s;

    gl_FragColor = vec4(rgb, tex.a * v_color0.a * u_baseTint.a);
}