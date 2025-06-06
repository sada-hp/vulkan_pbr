#include "common.glsl"
#include "constants.glsl"

#ifndef _LIGHTING_SHADER
#define _LIGHTING_SHADER

struct SAtmosphere
{
    vec3 L;
    vec3 S;
    vec3 E;
    vec3 T;
    float Shadow;
};

float HenyeyGreensteinPhase(float a, float g)
{
    return ONE_OVER_4PI * ((1.0 - g * g) / pow((1.0 + g * g - 2.0 * g * a), 1.5));
}

float RayleighPhase(float a)
{
    return 3.0 * (1 + a * a) / (16.0 * PI); 
}

float MiePhase(float a)
{
    return 1.5 * ONE_OVER_4PI * (1.0 - MieG * MieG) * pow(1.0 + (MieG * MieG) - 2.0 * MieG * a, -3.0/2.0) * (1.0 + a * a) / (2.0 + MieG * MieG);
}

float BeerLambert(float e, float ds)
{
    return exp(-e * ds);
}

float DualLobeFunction(float a, float g1, float g2, float w)
{
    return mix(HenyeyGreensteinPhase(a, g1), HenyeyGreensteinPhase(a, g2), w); 
}

float DrainePhase(float a, float g, float k)
{
    return HenyeyGreensteinPhase(a, g) * ((1 + k * a * a) / (1 + k * (1 + 2 * g * g) / 3.0));
}

//https://research.nvidia.com/labs/rtr/approximate-mie/publications/approximate-mie.pdf
float HGDPhase(float a, float ghg, float gd, float kd, float w)
{
    float dp = DrainePhase(a, gd, kd);
    float hgp = HenyeyGreensteinPhase(a, ghg);

    return mix(hgp, dp, w);
}

float HGDPhaseCloud(float a)
{
    float d = mix(5.0, 50.0, MieG);

    float ghg = exp(-0.0990567/(d - 1.67154));
    float gd = exp(-2.20679/(d + 3.91029) - 0.428934);
    float kd = exp(3.62489 - (8.29288/(d + 5.52825)));
    float w = exp(-0.599085/(d - 0.641583) - 0.665888);

    return HGDPhase(a, ghg, gd, kd, w);
}

float HGDPhaseCloud(float a, float g)
{
    float d = mix(5.0, 50.0, MieG);

    float ghg = exp(-0.0990567/(d - 1.67154));
    float gd = exp(-2.20679/(d + 3.91029) - 0.428934);
    float kd = exp(3.62489 - (8.29288/(d + 5.52825)));
    float w = exp(-0.599085/(d - 0.641583) - 0.665888);

    return HGDPhase(a, g * ghg, g * gd, kd, w);
}

float HGDPhase(float a)
{
    float d = mix(1.5, 5.0, 1.0 - MieG);

    float ghg = 0.0604931 * log(log(d)) + 0.940256;
    float gd =  0.500411 - 0.081287 / (-2 * log(d) + tan(log(d)) + 1.27551);
    float kd =  7.30354 * log(d) + 6.31675;
    float w =  0.026914 * (log(d) - cos(5.68947 * (log(log(d)) - 0.0292149))) + 0.376475;
    
    return HGDPhase(a, ghg, gd, kd, w);
}

float HGDPhase(float a, float g)
{
    float d = mix(1.5, 5.0, 1.0 - MieG);

    float ghg = 0.0604931 * log(log(d)) + 0.940256;
    float gd =  0.500411 - 0.081287 / (-2 * log(d) + tan(log(d)) + 1.27551);
    float kd =  7.30354 * log(d) + 6.31675;
    float w =  0.026914 * (log(d) - cos(5.68947 * (log(log(d)) - 0.0292149))) + 0.376475;
    
    return HGDPhase(a, g * ghg, g * gd, kd, w);
}

float Powder(float e, float ds)
{
    return 2.0 * exp(-ds * e) * (1.0 - exp(-ds * e * 2.0));
}

vec3 GetMie(vec4 RayMie)
{ 
    return RayMie.rgb * RayMie.w / max ( RayMie.r, 1e-4 ) * ( BetaR.r / BetaR );
}

vec4 GetInscattering(sampler3D LUT, float R, float Mu, float MuS, float DotVL)
{
    float H = sqrt ( Rt * Rt - Rg * Rg );
    float Rho = sqrt ( R * R - Rg * Rg );
    float RMu = R * Mu;
    float Delta = RMu * RMu - R * R + Rg * Rg;
    vec4 Cst = ((RMu < 0.0 && Delta > 0.0) ? vec4 (1.0, 0.0, 0.0, 0.5 - 0.5 / DIM_MU) : vec4(-1.0, H * H, H, 0.5 + 0.5 / DIM_MU));
    
    float uR = 0.5 / (DIM_R - 1.0) + Rho / H * ( 1.0 - 1.0 / (DIM_R - 1.0) );
    float uMu = Cst.w + (RMu * Cst.x + sqrt(Delta + Cst.y)) / (Rho + Cst.z) * (0.5 - 1.0 / DIM_MU);
    float uMuS = 0.5 / DIM_MU_S + (atan(max(MuS, -0.1975) * tan(1.26 * 1.1)) / 1.1 + (1.0 - 0.26)) * 0.5 * (1.0 - 1.0 / DIM_MU_S);
    float Weight = (DotVL + 1.0) * 0.5 * (DIM_NU - 1.0);
    float uNu = floor(Weight);

    return mix(texture(LUT, vec3((uNu + uMuS) / DIM_NU, uMu, uR)), texture(LUT, vec3((uNu + uMuS + 1.0) / DIM_NU, uMu, uR)), Weight - uNu);
}

vec3 GetIrradiance(sampler2D LUT, float R, float MuS)
{
    float uvR = (R - Rg) / (Rt - Rg);
    float uvMuS = (0.2 * max(MuS, 0.0)) / (1.0 + 0.2);
    return texture(LUT, vec2(uvMuS, uvR)).rgb;
}

vec3 GetTransmittance(sampler2D LUT, float R, float Mu)
{
    float uvR = sqrt((R - Rg) / (Rt - Rg));
    float uvMu = atan((Mu + 0.15) / (1.0 + 0.15) * tan(1.5)) / 1.5;
    return mix(vec3(1.0), texture(LUT, vec2(uvMu, uvR)).rgb, saturate((Rt - Rg) / (R - Rg)));
}

vec3 GetTransmittance(sampler2D LUT, float R, float Mu, float d)
{
    float R1 = sqrt(R * R + d * d + 2.0 * R * Mu * d);
    float Mu1 = (R * Mu + d) / R1;

    if (Mu > 0.0)
    {
        return min(GetTransmittance ( LUT, R, Mu ) / GetTransmittance ( LUT, R1, Mu1 ), 1.0 );
    }
    else
    {
        return min(GetTransmittance( LUT, R1, -Mu1 ) / GetTransmittance ( LUT, R, -Mu ), 1.0 );
    }
}

vec3 GetTransmittanceWithShadow(sampler2D LUT, float R, float Mu)
{
    const float eps = 0.04;
    float CosHorizon = -sqrt(1.0 - (Rg / R) * (Rg / R));

    if (Mu < CosHorizon)
    {
        return vec3(0.0);
    }
    else
    {
        return GetTransmittance(LUT, R, Mu);
    }
}

vec3 GetTransmittanceWithShadow(sampler2D LUT, float R, float Mu, float R1, float Mu1)
{
    const float eps = 0.04;
    float CosHorizon = -sqrt(1.0 - (Rg / R) * (Rg / R));

    if (Mu < CosHorizon)
    {
        return vec3(0.0);
    }
    else
    {
        return GetTransmittance(LUT, R, Mu) * GetTransmittance(LUT, R1, Mu1);
    }
}

vec3 GetTransmittance(sampler2D LUT, float R, float Mu, vec3 v, vec3 x0)
{
    float R1 = length ( x0 );
    float Mu1 = dot ( x0, v ) / R;

    if ( Mu > 0.0 )
    {
        return min(GetTransmittance ( LUT, R, Mu ) / GetTransmittance ( LUT, R1, Mu1 ), 1.0 );
    }
    else
    {
        return min(GetTransmittance(LUT, R1, -Mu1) / GetTransmittance(LUT, R, -Mu ), 1.0);
    }
}

// precomputed-atmospheric-scattering
void AerialPerspective(sampler2D TransmittanceLUT, sampler2D IrradianceLUT, sampler3D InscatteringLUT, vec3 Eye, vec3 Target, vec3 Sun, out SAtmosphere Atmosphere) 
{
    float Re = length(Eye);

    if (Re <= Rg + 1e-2) 
    {
        Atmosphere.S      = vec3(0.0);
        Atmosphere.L      = vec3(0.0);
        Atmosphere.E      = vec3(0.0);
        Atmosphere.T      = vec3(1.0);
        Atmosphere.Shadow = 1.0;
    }
    else
    {
        vec3 View = normalize(Target - Eye);
        float EdotV = dot(Eye, View) / Re;

#if 1
        float d = -Re * EdotV - sqrt(Re * Re * (EdotV * EdotV - 1.0) + (Rt + 2 * Rdelta) * (Rt + 2 * Rdelta));
        if (d > 0.0)
        {
            Eye = Eye + View * d;
            EdotV = (Re * EdotV + d) / (Rt + 2 * Rdelta);
            Re = Rt + 2 * Rdelta;
        }
#endif

        float Rp = max(length(Target), Rg);
        float VdotL = dot(View, Sun);
        float EdotL = dot(Eye, Sun) / Re;
        float PdotV = dot(Target, View) / Rp;
        float PdotL = dot(Target, Sun) / Rp;

        float PhaseR = RayleighPhase(VdotL);

#ifdef USE_CLOUD_MIE
        float PhaseM = HGDPhaseCloud(VdotL);
#else
        float PhaseM = MiePhase(VdotL);
#endif
        float Rpe = distance(Eye, Target);
        Atmosphere.T = GetTransmittance(TransmittanceLUT, Re, EdotV, View, Target);
        Atmosphere.L = GetTransmittanceWithShadow(TransmittanceLUT, Rp, PdotL, Re, EdotL);
        Atmosphere.E = GetIrradiance(IrradianceLUT, Rp, PdotL);

        const float EPS = 0.008;
        vec4 inscatter = vec4(0.0);
        float muHoriz = -sqrt(1.0 - (Rg / Re) * (Rg / Re));
        if (abs(EdotV - muHoriz) < EPS) 
        {
            float a = ((EdotV - muHoriz) + EPS) / (2.0 * EPS);

            EdotV = muHoriz - EPS;
            Rp = sqrt(Re * Re + Rpe * Rpe + 2.0 * Re * Rpe * EdotV);
            PdotV = (Re * EdotV + Rpe) / Rp;
            vec4 inScatter0 = GetInscattering(InscatteringLUT, Re, EdotV, EdotL, VdotL);
            vec4 inScatter1 = GetInscattering(InscatteringLUT, Rp, PdotV, PdotL, VdotL);
            inScatter1 = min(inScatter1, inScatter0);
            vec4 inScatterA = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

            EdotV = muHoriz + EPS;
            Rp = sqrt(Re * Re + Rpe * Rpe + 2.0 * Re * Rpe * EdotV);
            PdotV = (Re * EdotV + Rpe) / Rp;
            inScatter0 = GetInscattering(InscatteringLUT, Re, EdotV, EdotL, VdotL);
            inScatter1 = GetInscattering(InscatteringLUT, Rp, PdotV, PdotL, VdotL);
            inScatter1 = min(inScatter1, inScatter0);
            vec4 inScatterB = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

            inscatter = mix(inScatterA, inScatterB, a);
        }
        else
        {
            vec4 inScatter0 = GetInscattering(InscatteringLUT, Re, EdotV, EdotL, VdotL);
            vec4 inScatter1 = GetInscattering(InscatteringLUT, Rp, PdotV, PdotL, VdotL);
            inScatter1 = min(inScatter1, inScatter0);
            inscatter = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);
        }
        inscatter.w *= smoothstep(0.00, 0.02, EdotL);

        Atmosphere.Shadow = smootherstep(0.0, 1.0, saturate(PdotL + 0.15));
        Atmosphere.S = (max(inscatter.rgb * PhaseR, 0.0) + max(GetMie(inscatter) * PhaseM, 0.0));

        if (EdotL < 0.0)
        {
            Atmosphere.S = mix(1.0, 0.1, pow(abs(EdotL), 1.0 / 3.0)) * Atmosphere.S;
        }
    }
}

vec3 SkyScattering(sampler2D TransmittanceLUT, sampler3D InscatteringLUT, vec3 Eye, vec3 View, vec3 Sun)
{
    if (SphereMinDistance(Eye, View, vec3(0.0), Rg) > 0.0)
        return vec3(0.0);

    vec3 Color = vec3(0.0);
    float Re = length(Eye);
    float EdotV = saturateAngle(dot(Eye, View) / Re);
    float EdotL = saturateAngle(dot(Eye, Sun) / Re);
    float VdotL = dot(View, Sun);

    float PhaseR    = RayleighPhase(VdotL);
    float PhaseHGD  = HGDPhase(VdotL); 
    vec3 Transmittance = GetTransmittance(TransmittanceLUT, Re, EdotV);
    vec4 Scattering = max(GetInscattering(InscatteringLUT, Re, EdotV, EdotL, VdotL), 0.0);
    Scattering.w *= smoothstep(0.00, 0.02, EdotL);

    vec2 D = SphereDistances(Eye, View, vec3(0.0), Rt);
    if (D.x > 0.0 || D.y > 0.0)
    {
        if (Re < Rt)
        {
            Color += max(PhaseR * Scattering.rgb, 0.0) + max(mix(PhaseHGD, 0.1 * PhaseHGD, smootherstep(0.0, 1.0, VdotL)) *  GetMie(Scattering), 0.0);
        }
        else
        {
            Color += max(PhaseR * Scattering.rgb, 0.0) + max(PhaseHGD *  GetMie(Scattering), 0.0);
            Color *= smootherstep(0.0, 1.0, 1.0 - saturate(max(min(D.x, D.y) / max(D.x, D.y), 0.0)));
        }
    }

#ifndef SKY_SCATTER_ONLY   
    // Sun
    Color += TWO_PI * Transmittance * vec3(smoothstep(0.99995, 1.0, max(0.0, VdotL)));
#endif

    return Color;
}

#endif