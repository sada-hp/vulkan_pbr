#include "common.glsl"
#include "constants.glsl"

float SphereDistance(vec3 ro, vec3 rd, vec3 so, float radius)
{
	vec3 sphereCenter = so;

	float radius2 = radius*radius;

	vec3 L = ro - sphereCenter;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius2;

	float discr = b * b - 4.0 * a * c;

	if (discr < 0.0) 
        return 0.0;

	return max(0.0, (-b + sqrt(discr))/2);
}

bool RaySphereintersection(vec3 ro, vec3 rd, vec3 so, float radius, out vec3 p1)
{
	vec3 sphereCenter = so;

	float radius2 = radius*radius;

	vec3 L = ro - sphereCenter;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius2;

	float discr = b * b - 4.0 * a * c;

	if (discr < 0.0) 
        return false;

	float t1 = max(0.0, (-b + sqrt(discr))/2);
    //float t2 = max(0.0, (-b - sqrt(discr))/2);
	if(t1 == 0.0){
		return false;
	}

    p1 = ro + rd * t1;

	return true;
}

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

float BeerLambert(float d)
{
    return exp(-d);
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
    //float d = mix(5.0, 50.0, g);
    float d = 25.0;

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

float Powder(float s, float d, float a)
{
    return 2.0 * (1.0 - exp(-d * s * 2)) * exp(-d * a * s);
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
    float uvMuS = (MuS + 0.2) / (1.0 + 0.2);
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
    const float eps = 0.1;
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

void PointRadiance(sampler2D TransmittanceLUT, sampler3D InscatteringLUT, vec3 Sun, vec3 Eye, vec3 Point, out SAtmosphere Atmosphere)
{
    Eye += 50.0;

    vec3 V = normalize(Point - Eye);

    float Re = length(Eye);
    float Rp = max(length(Point), Rg);

    float DotEV = dot(Eye, V) / Re;
    float DotPV = dot(Point, V) / Rp;

    float DotEL = dot(Eye, Sun) / Re;
    float DotPL = dot(Point, Sun) / Rp;

    float DotVL = dot(V, Sun);

    float Rl = RayleighPhase(DotVL);
    float Rm = MiePhase(DotVL);

    vec4 Inscattering = vec4(0.0);
    Atmosphere.T = GetTransmittance(TransmittanceLUT, Re, DotEV, V, Point);
    Atmosphere.L = GetTransmittanceWithShadow(TransmittanceLUT, Rp, DotPL);

    const float Eps = 0.005f;
    float MuHorizon = -sqrt(1.0 - pow(Rg / Re, 2.0));
    if (abs(DotEV - MuHorizon) < Eps)
    {
        float Mu = MuHorizon - Eps;
        float Rpe = distance(Eye, Point);
        float H = sqrt(Re * Re + Rpe * Rpe + 2.0f * Re * Rpe * Mu);
        float MuSample = (Re * Mu + Rpe) / H;

        vec4 InscatteringAL = GetInscattering(InscatteringLUT, Re, Mu, DotEL, DotVL);
        vec4 InscatteringBL = GetInscattering(InscatteringLUT, H, MuSample, DotPL, DotVL);
        vec4 Inscattering0 = max(InscatteringAL - Atmosphere.T.rgbr * InscatteringBL, 0.0);

        Mu = MuHorizon + Eps;
        H = sqrt(Re * Re + Rpe * Rpe + 2.0f * Re * Rpe * Mu);
        MuSample = (Re * Mu + Rpe) / H;

        InscatteringAL = GetInscattering(InscatteringLUT, Re, Mu, DotEL, DotVL);
        InscatteringBL = GetInscattering(InscatteringLUT, H, MuSample, DotPL, DotVL);
        vec4 Inscattering1 = max(InscatteringAL - Atmosphere.T.rgbr * InscatteringBL, 0.0f);

        float t = ((DotEV - MuHorizon) + Eps) / (2.0 * Eps);
        Inscattering = mix(Inscattering0, Inscattering1, t);
    }
    else
    {
        vec4 InscatteringAL = GetInscattering(InscatteringLUT, Re, DotEV, DotEL, DotVL);
        vec4 InscatteringBL = GetInscattering(InscatteringLUT, Rp, DotPV, DotPL, DotVL);
        Inscattering = max(max(InscatteringAL, 0.0) - Atmosphere.T.rgbr * max(InscatteringBL, 0.0), 0.0);
    }

    // avoids imprecision problems in Mie scattering when Sun is below horizon
    Inscattering.w *= smoothstep(0.00, 0.02, DotEL);
    Atmosphere.S = max(Inscattering.rgb * Rl + GetMie(Inscattering) * Rm, 0.0) * MaxLightIntensity;

    const float LightInstensity = MaxLightIntensity / PI;

    Atmosphere.L *= Atmosphere.T * LightInstensity;
}