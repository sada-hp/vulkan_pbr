#define LUT_MEASURES
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

float UVFromUnitRange(float x, float size)
{
    return 0.5 / size + x * (1.0 - 1.0 / size);
}

float UnitRangeFromUV(float u, float size)
{
    return (u - 0.5 / size) / (1.0 - 1.0 /size);
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

void UVWToWorldInscatter(vec2 UV, float Layer, out float R, out float CosViewZenith, out float CosSunZenith, out float CosViewun)
{
    const float dHdH = Rt * Rt - Rg * Rg;

    R = Layer / float(DIM_R - 1.0);
    R = R * R;
    R = sqrt(Rg * Rg + R * R * dHdH) + (Layer == 0 ? 0.01 : ((Layer == DIM_R - 1) ? -0.001 : 0.0));

    float x = gl_GlobalInvocationID.x;
    float y = gl_GlobalInvocationID.y;

    if (UV.y < 0.5)
    {
        float Dmax = sqrt(R * R - Rg * Rg);
        float D = 1.0 - y / (DIM_MU * 0.5 - 1.0);
        D = clamp(R - Rg, D * Dmax, Dmax * 0.999);
        CosViewZenith = (Rg * Rg - R * R - D * D) / (2.0 * R * D);
        CosViewZenith = min(CosViewZenith, -sqrt(1.0 - (Rg / R) * (Rg / R)) - 0.001);
    }
    else
    {
        float Dmax = sqrt(R * R - Rg * Rg) + sqrt(dHdH);
        float D = (y - DIM_MU * 0.5) / (DIM_MU * 0.5 - 1.0);
        D = clamp(Rt - R, D * Dmax, Dmax * 0.999);
        CosViewZenith = (Rt * Rt - R * R - D * D) / (2.0 * R * D);
    }

    CosSunZenith = mod(x, float(DIM_MU_S)) / (DIM_MU_S - 1.0);
    CosSunZenith = tan((2.0 * CosSunZenith - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);

    CosViewun = -1.0 + floor(x / float(DIM_MU_S)) / (DIM_NU - 1.0) * 2.0;
}