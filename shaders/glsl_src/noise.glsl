//Hash Functions for GPU Rendering (http://www.jcgt.org/published/0009/03/02/)
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

uint NOISE_SEED = 2798796415U;

float noise(float p)
{
    return fract(sin(p + 1.951) * 43758.5453);
}

float noise(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2 noise2(vec2 p)
{
    ivec2 n = ivec2(p.x*ivec2(3,37) + p.y*ivec2(311,113));
	n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return -1.0+2.0*vec2( n & ivec2(0x0fffffff))/float(0x0fffffff);
}

vec2 pcg2d(vec2 p)
{
    uvec2 v = uvec2(p) * 1664525u + NOISE_SEED;

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v >> 16u);

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v>>16u);

    return vec2(v) / float(0xffffffffu);
}

vec3 pcg3d(vec3 p)
{
    uvec3 v = uvec3(p) * 1664525u + NOISE_SEED;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    v = ((v >> int((v >> 28u) + 4u)) ^ v) * 277803737u;
	v = v ^ (v >> 16u);

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    return vec3(v) / float(0xffffffffu);
}

float worley(vec2 p0, float freq)
{
    float min_d = 1.0;
    const vec2 i = floor(p0 * freq);
    const vec2 f = fract(p0 * freq);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 p1 = vec2(x, y);
            vec2 neighbour_cell = mod(i + p1, freq);
            min_d = min(min_d, distance(pcg2d(neighbour_cell), f - p1));
        }
    }

    return min_d;
}

float worley(vec3 p0, float freq)
{
    float min_d = 1.0;
    const vec3 i = floor(p0 * freq);
    const vec3 f = fract(p0 * freq);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                vec3 p1 = vec3(x, y, z);
                vec3 neighbour_cell = mod(i + p1, freq);
                min_d = min(min_d, distance(pcg3d(neighbour_cell), f - p1));
            }
        }
    }

    return min_d;
}

float perlin(vec2 x0, float freq) 
{
    vec2 i = floor(x0 * freq);
    vec2 f = fract(x0 * freq);

    vec2 of = vec2(0.0, 1.0);

    vec2 ga = noise2(mod(i + of.xx, freq));
    vec2 gb = noise2(mod(i + of.yx, freq));
    vec2 gc = noise2(mod(i + of.xy, freq));
    vec2 gd = noise2(mod(i + of.yy, freq));
    
    float va = dot( ga, f - of.xx );
    float vb = dot( gb, f - of.yx );
    float vc = dot( gc, f - of.xy );
    float vd = dot( gd, f - of.yy );

    vec2 u = smoothstep(0.0, 1.0, f);
    
    return mix(mix(va, vb, u.x), mix(vc, vd, u.x), u.y);
}

float perlin(vec3 x0, float freq) 
{
    vec3 i = floor(x0 * freq);
    vec3 f = fract(x0 * freq);
    
    vec2 of = vec2(0.0, 1.0);

    vec3 ga = pcg3d(mod(i + of.xxx, freq));
    vec3 gb = pcg3d(mod(i + of.yxx, freq));
    vec3 gc = pcg3d(mod(i + of.xyx, freq));
    vec3 gd = pcg3d(mod(i + of.yyx, freq));
    vec3 ge = pcg3d(mod(i + of.xxy, freq));
    vec3 gf = pcg3d(mod(i + of.yxy, freq));
    vec3 gg = pcg3d(mod(i + of.xyy, freq));
    vec3 gh = pcg3d(mod(i + of.yyy, freq));

    float va = dot(ga, f);
    float vb = dot(gb, f - of.yxx);
    float vc = dot(gc, f - of.xyx);
    float vd = dot(gd, f - of.yyx);
    float ve = dot(ge, f - of.xxy);
    float vf = dot(gf, f - of.yxy);
    float vg = dot(gg, f - of.xyy);
    float vh = dot(gh, f - of.yyy);
    
    vec3 u = smoothstep(0.0, 1.0, f);
    
    return va + 
           u.x * (vb - va) + 
           u.y * (vc - va) + 
           u.z * (ve - va) + 
           u.x * u.y * (va - vb - vc + vd) + 
           u.y * u.z * (va - vc - ve + vg) + 
           u.z * u.x * (va - vb - ve + vf) + 
           u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
}

float fbm_worley(vec2 x, float f, uint n) 
{
    float v = 0.0;
	float a = 0.5;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * worley(x, f);
        f *= 2.0;

        as += a;
		a *= a;
	}
    
	return clamp(v / as * 0.5 + 0.5, -1.0, 1.0);
}

float fbm_worley(vec3 x, float f, uint n) 
{
    float v = 0.0;
	float a = 0.5;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * worley(x, f);
        f *= 2.0;

        as += a;
		a *= a;
	}
    
	return clamp(v / as * 0.5 + 0.5, -1.0, 1.0);
}

float fbm_perlin(vec2 x, float f, uint n) 
{
	float v = 0.0;
	float a = 0.5;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
        f *= 2.0;

        as += a;
		a *= a;
	}

	return clamp(v / as * 0.5 + 0.5, -1.0, 1.0);
}

float fbm_perlin(vec3 x, float f, uint n) 
{
	float v = 0.0;
	float a = 0.5;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
        f *= 2.0;

        as += a;
		a *= a;
	}

	return clamp(v / as * 0.5 + 0.5, -1.0, 1.0);
}