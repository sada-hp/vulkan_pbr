#include "../lighting.glsl"

float DistanceToAtmosphere(float R, float Mu)
{        
    float Dout = -R * Mu + sqrt ( R * R * ( Mu * Mu - 1.0 ) + Rt * Rt );
    float Delta2 = R * R * ( Mu * Mu - 1.0 ) + Rg * Rg;
    
    if ( Delta2 >= 0.0 )
    {
        float Din = -R * Mu - sqrt ( Delta2 );
        
        if ( Din >= 0.0 )
        {
            Dout = min ( Dout, Din );
        }
    }

    return Dout;
}

float GetAirDensity(float R, float H)
{
    return exp(-(R - Rg) / H);
}

void GetTransmittanceRMu(vec2 UV, out float R, out float Mu)
{
    R = Rg + (UV.y * UV.y) * (Rt - Rg);
    Mu = -0.15 + tan (1.5 * UV.x) / tan (1.5) * (1.0 + 0.15);
}

void GetIrradienceRMu(vec2 UV, out float R, out float Mu)
{
    R = Rg + UV.y * (Rt - Rg);
    Mu = -0.2 + UV.x * (1.0 + 0.2);
}

vec2 GetUV(ivec2 size)
{
    vec2 UV = (vec2(gl_GlobalInvocationID.xy) + 0.5) / vec2(size - 1);
    // UV.y = 1.0 - UV.y;

    return clamp(UV, 0.0, 1.0);
}

vec2 GetUV(ivec2 size, vec2 offset)
{
    vec2 UV = (vec2(gl_GlobalInvocationID.xy) + 0.5 + offset) / vec2(size - 1);
    // UV.y = 1.0 - UV.y;

    return clamp(UV, 0.0, 1.0);
}

vec3 GetUV(ivec3 size)
{
    vec3 UV = (vec3(gl_GlobalInvocationID.xyz) + 0.5) / vec3(size - 1);
    // UV.y = 1.0 - UV.y;

    return clamp(UV, 0.0, 1.0);
}

vec3 GetUV(ivec3 size, vec3 offset)
{
    vec3 UV = (vec3(gl_GlobalInvocationID.xyz) + 0.5 + offset) / vec3(size - 1);
    // UV.y = 1.0 - UV.y;

    return clamp(UV, 0.0, 1.0);
}

void UVWToWorldInscatter(vec3 UV, out float R, out float CosViewZenith, out float CosSunZenith, out float CosViewun)
{
    const float dHdH = Rt * Rt - Rg * Rg;

    R = sqrt(Rg * Rg + UV.z * UV.z * dHdH) + (UV.z == 0 ? 0.01 : ((UV.z == 1.0) ? -0.001 : 0.0));

    const float Dmin = Rt - R;
    const float Dmax = sqrt(R * R - Rg * Rg) + sqrt(Rt * Rt - Rg * Rg);
    const float Dminp = R - Rg;
    const float Dmaxp = sqrt(R * R - Rg * Rg);

    float x = float(gl_GlobalInvocationID.x);
    float y = float(gl_GlobalInvocationID.y);

    if (y < float(DIM_MU) / 2.0)
    {
        float D = 1.0 - y / (float(DIM_MU) / 2.0 - 1.0);
        D = min(max(Dminp, D * Dmaxp), Dmaxp * 0.999);

        CosViewZenith = (Rg * Rg - R * R - D * D) / (2.0 * R * D);
        CosViewZenith = min(CosViewZenith, -sqrt(1.0 - (Rg / R) * (Rg / R)) - 0.001);
    }
    else
    {
        float D = (y - float(DIM_MU) / 2.0) / (float(DIM_MU) / 2.0 - 1.0);
        D = clamp(Rt - R, D * Dmax, Dmax * 0.999);
        CosViewZenith = (Rt * Rt - R * R - D * D) / (2.0 * R * D);
    }

    CosSunZenith = mod(x, float(DIM_MU_S)) / (float(DIM_MU_S) - 1.0);
    CosSunZenith = tan((2.0 * CosSunZenith - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);

    CosViewun = -1.0 + floor(x / float(DIM_MU_S)) / (float(DIM_NU) - 1.0) * 2.0;
}