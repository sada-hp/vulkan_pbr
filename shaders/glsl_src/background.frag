#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 2) uniform sampler3D InscatteringLUT;

vec4 GetInscattering(sampler3D LUT, float Rg, float Rt, float R, float Mu, float MuS, float DotVL)
{
    float H = sqrt ( Rt * Rt - Rg * Rg );
    float Rho = sqrt ( R * R - Rg * Rg );
    float RMu = R * Mu;
    float Delta = RMu * RMu - R * R + Rg * Rg;
    vec4 Cst = ((RMu < 0.0 && Delta > 0.0) ? vec4 (1.0, 0.0, 0.0, 0.5 - 0.5 / DIM_MU) : vec4(-1.0, H * H, H, 0.5 + 0.5 / DIM_MU));
    
    float uR = 0.5 / DIM_R + Rho / H * ( 1.0 - 1.0 / DIM_R );
    float uMu = Cst.w + (RMu * Cst.x + sqrt(Delta + Cst.y)) / (Rho + Cst.z) * (0.5 - 1.0 / DIM_MU);
    float uMuS = 0.5 / DIM_MU_S + (atan(max(MuS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5 * (1.0 - 1.0 / DIM_MU_S);
    float Weight = (DotVL + 1.0) * 0.5 * (DIM_NU - 1.0);
    float uNu = floor(Weight);

    return mix (texture(LUT, vec3((uNu + uMuS) / DIM_NU, uMu, uR)), texture(LUT, vec3((uNu + uMuS + 1.0) / DIM_NU, uMu, uR)), Weight - uNu);
}

vec3 GetMie ( vec4 RayMie )
{ 
    return RayMie.rgb * RayMie.w / max ( RayMie.r, 1e-4 ) * ( BetaR.r / BetaR );
}

vec3 SkyRadiance(vec3 Sun, vec3 Eye, vec3 View)
{
    const float Re = length(Eye);

    const float DotEV = max((dot(Eye, View) / Re), 0.01);
    const float DotEL = dot ( Eye, Sun ) / Re;
    const float DotVL = dot ( View, Sun );

    const float PhaseR = RayleighPhase ( DotVL );
    const float PhaseM = HGDPhase ( DotVL, MieG );

    vec4 Inscattering = max ( GetInscattering ( InscatteringLUT, Rg, Rt, Re, DotEV, DotEL, DotVL ), 0.0 );

    return max ( Inscattering.rgb * PhaseR + GetMie ( Inscattering ) * PhaseM, 0.0 ) * MaxLightIntensity;
}

void main()
{
	vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

	float sunAmount = max(dot(RayDirection, ubo.SunDirection.xyz), 0.0);
    vec3 L = SkyRadiance(ubo.SunDirection.xyz,  vec3(0, Rg, 0), RayDirection.xyz) + 15.0 * vec3(smoothstep(0.9998, 1.0, sunAmount));
    outColor = vec4(L, 0.0);

    gl_FragDepth = 1.0;
}
