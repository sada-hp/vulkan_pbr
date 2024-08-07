#version 460
#include "ubo.glsl"
#include "lighting.glsl"

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 1) uniform sampler3D InscatteringLUT;

vec3 SkyRadiance(vec3 Sun, vec3 Eye, vec3 View)
{
    const float Re = length(Eye);
    float DotEV = dot(Eye, View) / Re;
    float d = -Re * DotEV - sqrt(Re * Re * (DotEV * DotEV - 1.0) + Rt * Rt);

    if (d > 0.0) 
    {
        Eye += d * View;
        DotEV = (Re * DotEV + d) / Rt;
    }

    if (Re <= Rt) 
    {
        const float DotEL = dot(Eye, Sun) / Re;
        const float DotVL = dot(View, Sun);

        const float PhaseR = RayleighPhase(DotVL);
        const float PhaseM = HGDPhase(DotVL, MieG);
        
        vec4 Inscattering = max(GetInscattering(InscatteringLUT, Re, DotEV, DotEL, DotVL), 0.0 );
        return max(Inscattering.rgb * PhaseR + GetMie(Inscattering) * PhaseM, 0.0) * MaxLightIntensity;
    } 
    else 
    {
        return vec3(0.0);
    }
}

vec3 GetSunColor(vec3 Eye, vec3 V, vec3 S)
{
    vec3 p;
    return RaySphereintersection(Eye, V, vec3(0.0), Rg, p) ? vec3(0.0) : 10.0 * vec3(smoothstep(0.9998, 1.0, max(dot(V, S), 0.0)));
}

void main()
{
	vec2 ScreenUV = gl_FragCoord.xy / ubo.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);

    vec3 V = normalize(ScreenWorld.xyz);
    vec3 S = normalize(ubo.SunDirection.xyz);
    vec3 Eye = ubo.CameraPosition.xyz + vec3(0.0, Rg, 0.0);
    
    vec3 SkyColor = SkyRadiance(S, Eye, V);
    vec3 SunColor = GetSunColor(Eye, V, S);

    vec3 L = SkyColor + SunColor;
    outColor = vec4(L, 0.0);

    gl_FragDepth = 1.0;
}