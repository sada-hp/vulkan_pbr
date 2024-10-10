layout (constant_id = 0) const float planet_radius = 6360.0 * 1e3;
layout (constant_id = 1) const float atmosphere_radius = 6420.0 * 1e3;

#define PI 3.14159265359
#define ONE_OVER_PI 1.0 / PI
#define ONE_OVER_4PI 1.0 / (4.0 * PI)

#define TRANSMITTANCE_SAMPLES 500
#define INSCATTERING_SAMPLES 50
#define INSCATTERING_SPHERE_SAMPLES 16
#define IRRADIANCE_SAMPLES 32

#ifdef LUT_MEASURES
const float Rg = 6360.0;
const float Rt = 6420.0;
#else
const float Rg = planet_radius;
const float Rt = atmosphere_radius;
#endif

float Rdelta = atmosphere_radius - planet_radius;
float Rcb = Rg + 0.15 * Rdelta;
float Rct = Rg + 0.55 * Rdelta;
float Rcdelta = Rct - Rcb;

const float HR = 8.0;
const vec3 BetaR = vec3(5.8e-3, 1.35e-2, 3.31e-2);

// clear sky
const float HM = 1.2;
const vec3 BetaMSca = vec3(4e-3);
const vec3 BetaMEx = BetaMSca / 0.9;
const float MieG = 0.8;

// partly cloudy
//const float HM = 3.0;
//const vec3 BetaMSca = vec3(3e-3);
//const vec3 BetaMEx = BetaMSca / 0.9;
//const float MieG = 0.65;

const float MaxLightIntensity = 50.0;
const int DIM_MU = 128;
const int DIM_MU_S = 32;
const int DIM_R = 32;
const int DIM_NU = 8;