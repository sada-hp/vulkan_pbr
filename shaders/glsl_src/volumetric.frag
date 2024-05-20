#version 460
#include "ubo.glsl"
#include "lighting.glsl"
#include "noise.glsl"
#include "common.glsl"

vec3 sphereStart;
vec3 sphereEnd;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform CloudLayer
{
    float Coverage;
    float VerticalSpan;
    float Absorption;
    float PhaseCoefficient;
    float WindSpeed;
} Clouds;

layout(binding = 3) uniform sampler3D CloudLowFrequency;
layout(binding = 4) uniform sampler3D CloudHighFrequency;
layout(binding = 5) uniform sampler2D GradientTexture;
layout(binding = 6) uniform sampler2D TransmittanceLUT;
layout(binding = 7) uniform sampler2D IrradianceLUT;
layout(binding = 8) uniform sampler3D InscatteringLUT;

vec3 GetMie ( vec4 RayMie )
{ 
    return RayMie.rgb * RayMie.w / max ( RayMie.r, 1e-4 ) * ( BetaR.r / BetaR );
}

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

vec3 GetIrradiance(sampler2D LUT, float Rg, float Rt, float R, float MuS)
{
    float uvR = ( R - Rg ) / ( Rt - Rg );
    float uvMuS = ( MuS + 0.2 ) / ( 1.0 + 0.2 );
    return texture ( LUT, vec2 ( uvMuS, uvR ) ).rgb;
}

vec3 GetTransmittance ( sampler2D LUT, float Rg, float Rt, float R, float Mu )
{
    float uvR = sqrt ( ( R - Rg ) / ( Rt - Rg ) );
    float uvMu = atan ( ( Mu + 0.15 ) / ( 1.0 + 0.15 ) * tan ( 1.5 ) ) / 1.5;
    return texture ( LUT, vec2 ( uvMu, uvR ) ).rgb;
}

vec3 GetTransmittanceWithShadow ( sampler2D LUT, float Rg, float Rt, float R, float Mu )
{
    float CosHorizon = -sqrt ( 1.0 - ( Rg / R ) * ( Rg / R ) );
    return Mu < CosHorizon ? vec3 ( 0.0 ) : GetTransmittance ( LUT, Rg, Rt, R, Mu );
}

vec3 GetTransmittance(sampler2D LUT, float Rg, float Rt, float R, float Mu, float d)
{
    float R1 = sqrt(R * R + d * d + 2.0 * R * Mu * d);
    float Mu1 = (R * Mu + d) / R1;

    if (Mu > 0.0)
    {
        return min(GetTransmittance ( LUT, Rg, Rt, R, Mu ) / GetTransmittance ( LUT, Rg, Rt, R1, Mu1 ), 1.0 );
    }
    else
    {
        return min(GetTransmittance( LUT, Rg, Rt, R1, -Mu1 ) / GetTransmittance ( LUT, Rg, Rt, R, -Mu ), 1.0 );
    }
}

vec3 GetTransmittance(sampler2D LUT, float Rg, float Rt, float R, float Mu, vec3 v, vec3 x0)
{
    float R1 = length ( x0 );
    float Mu1 = dot ( x0, v ) / R;

    if ( Mu > 0.0 )
    {
        return min ( GetTransmittance ( LUT, Rg, Rt, R, Mu ) /
                        GetTransmittance ( LUT, Rg, Rt, R1, Mu1 ), 1.0 );
    }
    else
    {
        return min ( GetTransmittance ( LUT, Rg, Rt, R1, -Mu1 ) /
                        GetTransmittance ( LUT, Rg, Rt, R, -Mu ), 1.0 );
    }
}

void PointRadiance(sampler2D TransmittanceLUT, sampler2D IrradianceLUT, sampler3D InscatteringLUT, vec3 Sun, vec3 View, vec3 Eye, vec3 Point, out vec3 L, out vec3 T, out vec3 E, out vec3 S)
{
    const float Re = length(Eye);
    const float Rp = max(length(Point), Re);

    float DotEV = dot(Eye, View) / Re;

    float DotPV = dot(Point, View) / Rp;

    const float DotEL = dot ( Eye, Sun ) / Re;
    const float DotPL = dot ( Point, Sun ) / Rp;

    const float DotVL = dot ( View, Sun );

    const float PhaseR = RayleighPhase ( DotVL );
    const float PhaseM = HGDPhase ( DotVL, MieG );

    T = GetTransmittance ( TransmittanceLUT, Rg, Rt, Re, DotEV, View, Point );
        
    L = GetTransmittanceWithShadow ( TransmittanceLUT, Rg, Rt, Rp, DotPL );
    E = GetIrradiance ( IrradianceLUT, Rg, Rt, Rp, DotPL );

    vec4 InscatteringAL = max(GetInscattering ( InscatteringLUT, Rg, Rt, Re, DotEV, DotEL, DotVL ), GetInscattering(InscatteringLUT, Rg, Rt, Re, -DotEV, DotEL, DotVL));
    vec4 InscatteringBL = max(GetInscattering(InscatteringLUT, Rg, Rt, Rp, DotPV, DotPL, DotVL), GetInscattering(InscatteringLUT, Rg, Rt, Rp, -DotPV, DotPL, DotVL));
    vec4 Inscattering = max(max(InscatteringAL, 0.0) - T.rgbr * max(InscatteringBL, 0.0 ), 0.0);

    // avoids imprecision problems in Mie scattering when Sun is below horizon
    Inscattering.w *= smoothstep ( 0.00, 0.02, DotEL );

    const float LightInstensity = MaxLightIntensity / PI;

    S = max ( Inscattering.rgb * PhaseR + GetMie ( Inscattering ) * PhaseM, 0.0 ) * MaxLightIntensity;


    L *= T * LightInstensity;
    E *= T * LightInstensity;
}

float GetHeightFraction(vec3 p)
{
    //return saturate((p.y - sphereStart.y) / (sphereEnd.y - sphereStart.y));
    return 1.0 - saturate(dot(p, p) / dot(sphereEnd, sphereEnd));
}

vec3 GetUV(vec3 p, float scale)
{
    return scale * p / ATMOSPHERE_START + vec2(ubo.Time * Clouds.WindSpeed * 0.01, 0.0).xyx;
}

float SampleCloudShape(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 4.0);

    vec4 low_frequency_noise = textureLod(CloudLowFrequency, uv, lod);
    float base = low_frequency_noise.r;
    base = remap(base, 1.0 - Clouds.Coverage, 1.0, 0.0, 1.0);

    base *= saturate(remap(height, 0.1, 0.2, 0.0, 1.0));
    base *= saturate(remap(height, mix(0.8, 0.5, Clouds.VerticalSpan), mix(0.9, 0.6, Clouds.VerticalSpan), 0.0, 1.0));
    base *= Clouds.Coverage;

    return base;
}

//GPU Pro 7
float SampleDensity(vec3 x0, int lod)
{
    float height = GetHeightFraction(x0);
    vec3 uv =  GetUV(x0, 60.0);

    float base = SampleCloudShape(x0, lod);

    //return base;

    vec4 high_frequency_noise = texture(CloudHighFrequency, uv);
    float high_frequency_fbm = high_frequency_noise.r * 0.625 + high_frequency_noise.g * 0.25 + high_frequency_noise.b * 0.125;
    float high_frequency_modifier = mix(high_frequency_fbm, 1.0 - high_frequency_fbm, saturate(height * 45.0));

    return remap(base, high_frequency_modifier * 0.1, 1.0, 0.0, 1.0);
}

vec3 light_kernel[] =
{
    vec3(0.5, 0.5, 0.5),
    vec3(-0.628, 0.64, 0.234),
    vec3(0.23, -0.123, 0.75),
    vec3(0.98, 0.45, -0.32),
    vec3(-0.1, -0.54, 0.945),
    vec3(0.0, 0.6, -0.78),
};

float MarchToLight(vec3 rs, vec3 rd, float stepsize)
{
    vec3 pos = rs;
    float density = 0.0;
    float radius = 1.0;
    float transmittance = 1.0;

    stepsize *= 6.f;
    for (int i = 0; i <= 6; i++)
    {
        pos = rs + float(i) * radius * light_kernel[i];

        float sampled_density = SampleDensity(pos, 2);

        if (sampled_density > 0.0)
        {
            float transmittance_light = BeerLambert(stepsize * sampled_density * Clouds.Absorption);
            transmittance *= transmittance_light;
        }
        
        density += sampled_density;
        rs += stepsize * rd;
        radius += 1.f / 6.f;
    }

    return transmittance;
}

vec4 MarchToCloud(vec3 rs, vec3 re, vec3 rd)
{
    const int steps = 128;
    const float len = distance(rs, re);
    const float stepsize = len / float(steps);

    vec4 scattering = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 pos = rs;
    float density = 0.0;
    vec3 rl = ubo.SunDirection;
    //float phase = DualLobeFunction(dot(rl, rd), Clouds.PhaseCoefficient, -0.25, 0.55);
    float phase = HGDPhase(dot(rl, rd), 0.75, -0.25, 0.5, Clouds.PhaseCoefficient);

    int i = 0;
    while (density == 0.0 && i < steps)
    {
        density = SampleDensity(pos, 2);
        pos += stepsize * rd * 10.0;
        i += 10;
    }

    //super hacky, but somehow it works, gonna be changed
    vec3 L, T, S, E;
    PointRadiance(TransmittanceLUT, IrradianceLUT, InscatteringLUT, rl, rd, vec3(0, Rg, 0) + View.CameraPosition.xyz, vec3(0, Rg, 0) + View.CameraPosition.xyz, L, T, E, S);

    for (; i < steps; i++)
    {
        density = SampleDensity(pos, 0);

        if (density > 0.0) 
        {
            float extinction = max(density * Clouds.Absorption, 1e-8);
            float transmittance = BeerLambert(stepsize * extinction);

            vec3 energy = L * density * phase * (MarchToLight(pos, rl, stepsize) + E) * Powder(stepsize, density, Clouds.Absorption) + S;
            vec3 intScat = (energy - energy * transmittance) / density;

            scattering.rgb += intScat * scattering.a;
            scattering.a *= transmittance;
        }

        pos += stepsize * rd;
    }

    return scattering;
}

void main()
{
    vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, -1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

    if (dot(RayDirection, vec3(0.0, 1.0, 0.0)) < 0.0) 
    {
        outColor = vec4(0.0);
        discard;
    }

    vec3 RayOrigin = View.CameraPosition.xyz;
    vec3 SphereCenter = vec3(View.CameraPosition.x, -EARTH_RADIUS, View.CameraPosition.z);
    if (RaySphereintersection(RayOrigin, RayDirection, SphereCenter, ATMOSPHERE_START, sphereStart)
    && RaySphereintersection(RayOrigin, RayDirection, SphereCenter, ATMOSPHERE_END, sphereEnd)) 
    {
        outColor = MarchToCloud(sphereStart, sphereEnd, RayDirection);
    }
    else 
    {
        discard;
    }

    gl_FragDepth = 1.0;
}