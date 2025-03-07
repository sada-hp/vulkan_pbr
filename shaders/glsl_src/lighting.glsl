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
    return PI * mix(HenyeyGreensteinPhase(a, g1), HenyeyGreensteinPhase(a, g2), w); 
}

float DrainePhase(float a, float g, float k)
{
    return HenyeyGreensteinPhase(a, g) * ((1 + k * a * a) / (1 + k * (1 + 2 * g * g) / 3.0));
}

//https://research.nvidia.com/labs/rtr/approximate-mie/publications/approximate-mie.pdf
float HGDPhase(float a, float w)
{
    float d = mix(5.0, 50.0, 1.0 - MieG);
    //float d = 15.0;

    float ghg = exp(-0.0990567/(d - 1.67154));
    float gd = exp(-2.20679/(d + 3.91029) - 0.428934);
    float kd = exp(3.62489 - (8.29288/(d + 5.52825)));
    //float w = exp(-0.599085/(d - 0.641583) - 0.665888);

    float dp = DrainePhase(a, gd, kd);
    float hgp = HenyeyGreensteinPhase(a, ghg);

    return mix(hgp, dp, w);
}

float HGDPhase(float a, float ghg, float gd, float kd, float w)
{
    float dp = DrainePhase(a, gd, kd);
    float hgp = HenyeyGreensteinPhase(a, ghg);

    return mix(hgp, dp, w);
}

float Powder(float e, float ds)
{
    return exp(-ds * e) * (1.0 - exp(-ds * e * 2));
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
    
    float uR = 0.5 / DIM_R + Rho / H * ( 1.0 - 1.0 / DIM_R );
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
    return texture(LUT, vec2(uvMu, uvR)).rgb;
}

vec3 GetTransmittanceWithShadow(sampler2D LUT, float R, float Mu)
{
    const float eps = 0.01;
    float CosHorizon = -sqrt(1.0 - (Rg / R) * (Rg / R));

    if (Mu < CosHorizon)
    {
        return vec3(0.0);
    }
    else if (Mu < CosHorizon + eps)
    {
        return GetTransmittance(LUT, R, Mu) * mix(vec3(1.0), vec3(0.0), saturate((CosHorizon + eps - Mu) / eps));
    }
    else
    {
        return GetTransmittance(LUT, R, Mu);
    }
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

vec3 GetTransmittanceWithShadow(sampler2D TransmittanceLUT, vec3 x, vec3 s)
{
    float r = length(x);
    if (r <= Rt)
    {
        float muS = dot(x, s) / r;
        return GetTransmittanceWithShadow(TransmittanceLUT, r, muS);
    }

    return vec3(0.0);
}

// precomputed-atmospheric-scattering
void AtmosphereAtPoint(sampler2D TransmittanceLUT, sampler3D InscatteringLUT, vec3 x, float t, vec3 v, vec3 s, out SAtmosphere Atmosphere) 
{
    vec3 result;
    float r = length(x);

    float mu = dot(x, v) / r;
    float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + Rt * Rt);
    
    if (d > 0.0) 
    {
        x += d * v;
        t -= d;
        mu = (r * mu + d) / Rt;
        r = Rt;
    }

    if (r <= Rt)
    {
        float nu = dot(v, s);
        float muS = dot(x, s) / r;
        float phaseR = RayleighPhase(nu);
        float phaseM = MiePhase(nu);
        vec4 inscatter = max(GetInscattering(InscatteringLUT, r, mu, muS, nu), 0.0);

        if (t > 0.0) 
        {
            vec3 x0 = x + t * v;
            float r0 = length(x0);
            float rMu0 = dot(x0, v);
            float mu0 = rMu0 / r0;
            float muS0 = dot(x0, s) / r0;
            Atmosphere.T = GetTransmittance(TransmittanceLUT, r, mu, v, x0);
            Atmosphere.L = GetTransmittanceWithShadow(TransmittanceLUT, r, muS);

            if (r0 > Rg + 0.01) 
            {
                inscatter = max(inscatter - Atmosphere.T.rgbr * GetInscattering(InscatteringLUT, r0, mu0, muS0, nu), 0.0);

                const float EPS = 0.004;
                float muHoriz = -sqrt(1.0 - (Rg / r) * (Rg / r));
                if (abs(mu - muHoriz) < EPS) 
                {
                    float a = ((mu - muHoriz) + EPS) / (2.0 * EPS);

                    mu = muHoriz - EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    vec4 inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    vec4 inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterA = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    mu = muHoriz + EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterB = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    inscatter = mix(inScatterA, inScatterB, a);
                }
            }
        }
        inscatter.w *= smoothstep(0.00, 0.02, muS);

        result = max(inscatter.rgb * phaseR + GetMie(inscatter) * phaseM, 0.0);
    } 
    else 
    {
        Atmosphere.S = vec3(0.0);
        Atmosphere.L = vec3(0.0);
        Atmosphere.T = vec3(0.0);
        return;
    }

    Atmosphere.S = result * MaxLightIntensity;
}

void AtmosphereAtPoint_2(sampler2D TransmittanceLUT, sampler3D InscatteringLUT, vec3 x, float t, vec3 v, vec3 s, out SAtmosphere Atmosphere) 
{
    vec3 resultHG;
    vec3 resultHGD;
    float r = length(x);

    float mu = dot(x, v) / r;
    float d = -r * mu - sqrt(r * r * (mu * mu - 1.0) + Rt * Rt);
    
    if (d > 0.0) 
    {
        x += d * v;
        t -= d;
        mu = (r * mu + d) / Rt;
        r = Rt;
    }

    if (r <= Rt)
    {
        float nu = dot(v, s);
        float muS = dot(x, s) / r;
        float phaseR = RayleighPhase(nu);
        float phaseHGD = HGDPhase(nu, MieG);
        vec4 inscatter = max(GetInscattering(InscatteringLUT, r, mu, muS, nu), 0.0);

        if (t > 0.0) 
        {
            vec3 x0 = x + t * v;
            float r0 = length(x0);
            float rMu0 = dot(x0, v);
            float mu0 = rMu0 / r0;
            float muS0 = dot(x0, s) / r0;
            Atmosphere.T = GetTransmittance(TransmittanceLUT, r, mu, v, x0);
            Atmosphere.L = GetTransmittanceWithShadow(TransmittanceLUT, r, muS);

            if (r0 > Rg + 0.01) 
            {
                inscatter = max(inscatter - Atmosphere.T.rgbr * GetInscattering(InscatteringLUT, r0, mu0, muS0, nu), 0.0);

                const float EPS = 0.004;
                float muHoriz = -sqrt(1.0 - (Rg / r) * (Rg / r));
                if (abs(mu - muHoriz) < EPS) 
                {
                    float a = ((mu - muHoriz) + EPS) / (2.0 * EPS);

                    mu = muHoriz - EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    vec4 inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    vec4 inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterA = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    mu = muHoriz + EPS;
                    r0 = sqrt(r * r + t * t + 2.0 * r * t * mu);
                    mu0 = (r * mu + t) / r0;
                    inScatter0 = GetInscattering(InscatteringLUT, r, mu, muS, nu);
                    inScatter1 = GetInscattering(InscatteringLUT, r0, mu0, muS0, nu);
                    vec4 inScatterB = max(inScatter0 - Atmosphere.T.rgbr * inScatter1, 0.0);

                    inscatter = mix(inScatterA, inScatterB, a);
                }
            }
        }
        inscatter.w *= smoothstep(0.00, 0.02, muS);

        resultHGD = max(inscatter.rgb * phaseR + GetMie(inscatter) * phaseHGD, 0.0);
    } 
    else 
    {
        Atmosphere.S = vec3(0.0);
        Atmosphere.L = vec3(0.0);
        Atmosphere.T = vec3(0.0);
        return;
    }

    Atmosphere.S = resultHGD * MaxLightIntensity;
}

vec3 PointRadiance(sampler2D TransmittanceLUT, sampler2D IrradianceLUT, vec3 eye, vec3 x, vec3 S)
{
    vec3 Result = vec3(0.0);

    float t = distance(eye, x);
    if (t > 0.0)
    {
        vec3 x0 = eye + t * normalize(x - eye);
        float r0 = length(x0);
        vec3 N = x0 / r0;

        vec2 uv = vec2(atan(N.y, N.x), acos(N.z)) * vec2(0.5, 1.0) / PI + vec2(0.5, 0.0);
        uv.y = 1.0 - uv.y;

        vec4 reflectance = texture(IrradianceLUT, uv) * vec4(0.2, 0.2, 0.2, 1.0);

        if (r0 > Rg + 0.01) {
            reflectance = vec4(0.4, 0.4, 0.4, 0.0);
        }

        float muS = max(dot(N, S), 0.0);
        vec3 sunLight = GetTransmittanceWithShadow(TransmittanceLUT, r0, muS);
        vec3 groundSkyLight = GetIrradiance(IrradianceLUT, r0, muS);
        vec3 groundColor = (muS * sunLight + groundSkyLight) * MaxLightIntensity; // reflectance.rgb * 

        if (reflectance.w > 0.0)
        {

        }

        Result = groundColor;
    }

    return Result;
}

vec3 PointIrradiance(sampler2D IrradianceLUT, vec3 eye, vec3 x, vec3 S)
{
    vec3 Result = vec3(0.0);

    float t = distance(eye, x);
    if (t > 0.0)
    {
        vec3 x0 = eye + t * normalize(x - eye);
        float r0 = length(x0);
        vec3 N = x0 / r0;
        float muS = max(dot(N, S), 0.0);

        Result = GetIrradiance(IrradianceLUT, r0, muS);
    }

    return Result;
}

#endif