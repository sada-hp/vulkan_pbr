#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(location = 0) in vec2 ScreenUV;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1) uniform sampler2D TransmittanceLUT;
layout(set = 1, binding = 2) uniform sampler3D InscatteringLUT;

vec3 GetSunColor(vec3 Eye, vec3 V, vec3 S)
{
    vec3 p;
    return RaySphereintersection(Eye, V, vec3(0.0), Rg, p) ? vec3(0.0) : vec3(smoothstep(0.9998, 1.0, max(dot(V, S), 0.0)));
}

void main()
{
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = ubo.ProjectionMatrixInverse * ScreenNDC;
    vec4 ScreenWorld = vec4(ubo.ViewMatrixInverse * vec4(ScreenView.xy, -1.0, 0.0));

    vec3 V = normalize(ScreenWorld.xyz);
    vec3 S = normalize(ubo.SunDirection.xyz);
    vec3 Eye = ubo.CameraPosition.xyz;
    
    float t = SphereDistance(Eye, V, vec3(0.0), Rt, false);

    if (length(Eye) > Rt)
    {
        discard;
    }

    SAtmosphere Atmosphere;
    AtmosphereAtPointHGD(TransmittanceLUT, InscatteringLUT, Eye, t, V, S, Atmosphere);

    vec3 L = Atmosphere.S + Atmosphere.L * GetSunColor(Eye, V, S);
    outColor = vec4(L, 0.0);

    gl_FragDepth = 0.0;
}