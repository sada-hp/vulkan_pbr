#define PI 3.14159265359
#define ONE_OVER_4PI 1.0 / (4.0 * PI)
#define EARTH_RADIUS 6370e3
#define ATMOSPHERE_START 6500e3
#define ATMOSPHERE_END 6650e3
#define ATMOSPHERE_DELTA ATMOSPHERE_END - ATMOSPHERE_START
#define AMBIENT 0.06

float DistanceToAtmosphere(float r, float u, float atmo_r)
{
    float d = sqrt(max(pow(r, 2.0) * (pow(u, 2.0) - 1.0) + pow(atmo_r, 2.0), 0.0));
    return max(-r * u - d, 0.0);
}

bool raySphereintersection(vec3 ro, vec3 rd, float radius, out vec3 p1)
{
	vec3 sphereCenter = vec3(View.CameraPosition.x, -EARTH_RADIUS, View.CameraPosition.z);

	float radius2 = radius*radius;

	vec3 L = ro - sphereCenter;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L,L) - radius2;

	float discr = b*b - 4.0 * a * c;
	if(discr < 0.0) return false;
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
float HGDPhase(float a, float d)
{
    float ghg = exp(-0.0990567/(d - 1.67154));
    float gd = exp(-2.20679/(d + 3.91029) - 0.428934);
    float kd = exp(3.62489 - (8.29288/(d + 5.52825)));
    float w = exp(-0.599085/(d - 0.641583) - 0.665888);

    float dp = DrainePhase(a, gd, kd);
    float hgp = HenyeyGreensteinPhase(a, ghg);

    return (1.0 - w) * hgp + w * dp;
}

float Powder(float s, float d, float a)
{
    return 2.0 * (1.0 - exp(-d * s * 2)) * exp(-d * a * s);
}