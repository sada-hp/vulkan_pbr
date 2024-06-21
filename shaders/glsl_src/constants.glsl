#define PI 3.14159265359
#define ONE_OVER_PI 1.0 / PI
#define ONE_OVER_4PI 1.0 / (4.0 * PI)
#define EARTH_RADIUS 6370e3
#define ATMOSPHERE_START 6400e3
#define ATMOSPHERE_END 6450e3
#define ATMOSPHERE_DELTA ATMOSPHERE_END - ATMOSPHERE_START
#define AMBIENT 0.06

#define TRANSMITTANCE_SAMPLES 500
#define INSCATTERING_SAMPLES 50
#define INSCATTERING_SPHERE_SAMPLES 8
#define IRRADIANCE_SAMPLES 32

#ifdef LUT_MEASURES
const float Rg = 6360.0;
const float Rt = 6420.0;
#else
const float Rg = 6360.0 * 1e3;
const float Rt = 6420.0 * 1e3;
#endif

const float HR = 8.0;
const float HM = 1.2;

const vec3 BetaMSca = vec3(4e-3);
const vec3 BetaMEx = BetaMSca / 0.9;
const vec3 BetaR = vec3(5.8e-3, 1.35e-2, 3.31e-2);
const float MieG = 0.8;

const float MaxLightIntensity = 10.0;
const int DIM_MU = 128;
const int DIM_MU_S = 32;
const int DIM_R = 32;
const int DIM_NU = 8;