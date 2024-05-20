#include "../lighting.glsl"

float GetAirDensity ( float Rg, float R, float H )
{
    return exp ( -( R - Rg ) / H );
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

void GetIrradienceRMu(float Rg, float Rt, vec2 UV, out float R, out float Mu)
{
    R = Rg + UV.y * ( Rt - Rg );
    Mu = -0.2 + UV.x * ( 1.0 + 0.2 );
}

void UVWToWorldInscatter(float Rg, float Rt, vec2 UV, float Layer, out float R, out float CosViewZenith, out float CosSunZenith, out float CosViewun)
{
    const float dHdH = Rt * Rt - Rg * Rg;

    R = Layer / (DIM_R - 1.0);
    R = sqrt(Rg * Rg + R * R * dHdH) + (Layer == 0 ? 0.01 : ((Layer == DIM_R - 1) ? -0.001 : 0.0));

    float X = gl_GlobalInvocationID.x;
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

    CosSunZenith = mod(X, float(DIM_MU_S)) / (DIM_MU_S - 1.0);
    CosSunZenith = tan((2.0 * CosSunZenith - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);

    CosViewun = -1.0 + floor(X / float(DIM_MU_S)) / (DIM_NU - 1.0) * 2.0;
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

vec3 GetTransmittance ( sampler2D LUT, float Rg, float Rt, float R, float Mu, vec3 v, vec3 x0 )
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

vec3 GetIrradiance(sampler2D LUT, float Rg, float Rt, float R, float MuS)
{
    float uvR = ( R - Rg ) / ( Rt - Rg );
    float uvMuS = ( MuS + 0.2 ) / ( 1.0 + 0.2 );
    return texture ( LUT, vec2 ( uvMuS, uvR ) ).rgb;
}