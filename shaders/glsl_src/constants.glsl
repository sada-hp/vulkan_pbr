#ifndef _CONSTANTS_SHADER
#define _CONSTANTS_SHADER

layout (constant_id = 0) const float Rg = 6360.0 * 1e3;
layout (constant_id = 1) const float Rt = 6420.0 * 1e3;

#define TRANSMITTANCE_SAMPLES 500
#define INSCATTERING_SAMPLES 50
#define INSCATTERING_SPHERE_SAMPLES 16
#define IRRADIANCE_SAMPLES 32

float Rdelta = Rt - Rg;
float Rcb = Rg + 0.15 * Rdelta;
float Rct = Rg + 0.7 * Rdelta;
float Rcdelta = Rct - Rcb;

const float HR = 8.0;
const vec3 BetaR = vec3(5.8e-3, 1.35e-2, 3.31e-2);

#if 0
    // partly cloudy
    const float HM = 3.0;
    const vec3 BetaMSca = vec3(3e-3);
    const vec3 BetaMEx = BetaMSca / 0.9;
    const float MieG = 0.65;
#else
    // clear sky
    const float HM = 1.2;
    const vec3 BetaMSca = vec3(20e-3);
    const vec3 BetaMEx = BetaMSca / 0.9;
    const float MieG = 0.8;
#endif

const float MaxLightIntensity = 50.0;
const int DIM_MU = 128;
const int DIM_MU_S = 32;
const int DIM_R = 32;
const int DIM_NU = 8;

#endif