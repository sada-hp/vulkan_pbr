//Hash Functions for GPU Rendering (http://www.jcgt.org/published/0009/03/02/)
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

uint NOISE_SEED = 2798796415U;

float noise(vec2 p)
{
    return fract(abs(sin(dot(p, vec2(12.9898, 78.233)))) * 43758.5453);
}

float noise(vec3 p)
{
    p  = fract(p * .1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

vec2 noise2(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx+33.33);

    return fract((p3.xx+p3.yz)*p3.zy);
}

vec3 noise3(vec3 p)
{
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz+33.33);
    return fract((p.xxy + p.yxx)*p.zyx);
}

//Dave_Hoskins
//https://www.shadertoy.com/view/XdGfRR
vec3 noise32(vec3 p)
{
    uvec3 q = uvec3(ivec3(p)) * uvec3(1597334673U, 3812015801U, NOISE_SEED);
	q = (q.x ^ q.y ^ q.z)*uvec3(1597334673U, 3812015801U, NOISE_SEED);
	return vec3(q) * UIF;
}

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

float worley(vec2 p0, float freq)
{
    float min_d = 1.0;
    const vec2 i = floor(p0 * freq);
    const vec2 f = fract(p0 * freq);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 p1 = vec2(x, y);
            vec2 neigbour_cell = mod(i + p1, freq);
            min_d = min(min_d, distance(noise2(neigbour_cell), f - p1));
        }
    }

    return 1.0 - min_d;
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
                vec3 neigbour_cell = mod(i + p1, freq);
                min_d = min(min_d, distance(noise32(neigbour_cell), f - p1));
            }
        }
    }

    return 1.0 - min_d;
}

float perlin(vec2 x0, float freq) 
{
    vec2 i = floor(x0 * freq);
    vec2 f = fract(x0 * freq);

	float va = noise(i);
    float vb = noise(mod(i + vec2(1.0, 0.0), freq));
    float vc = noise(mod(i + vec2(0.0, 1.0), freq));
    float vd = noise(mod(i + vec2(1.0, 1.0), freq));

    vec2 u = smoothstep(0.0, 1.0, f);
    return mix(va, vb, u.x) + (vc - va) * u.y * (1.0 - u.x) + (vd - vb) * u.x * u.y;
}

float perlin(vec3 x0, float freq) 
{
    vec3 i = floor(x0 * freq);
    vec3 f = fract(x0 * freq);
    
    vec3 ga = noise32(i);
    vec3 gb = noise32(mod(i + vec3(1.0, 0.0, 0.0), freq));
    vec3 gc = noise32(mod(i + vec3(0.0, 1.0, 0.0), freq));
    vec3 gd = noise32(mod(i + vec3(1.0, 1.0, 0.0), freq));
    vec3 ge = noise32(mod(i + vec3(0.0, 0.0, 1.0), freq));
    vec3 gf = noise32(mod(i + vec3(1.0, 0.0, 1.0), freq));
    vec3 gg = noise32(mod(i + vec3(0.0, 1.0, 1.0), freq));
    vec3 gh = noise32(mod(i + vec3(1.0, 1.0, 1.0), freq));

    float va = dot(ga, f);
    float vb = dot(gb, f - vec3(1.0, 0.0, 0.0));
    float vc = dot(gc, f - vec3(0.0, 1.0, 0.0));
    float vd = dot(gd, f - vec3(1.0, 1.0, 0.0));
    float ve = dot(ge, f - vec3(0.0, 0.0, 1.0));
    float vf = dot(gf, f - vec3(1.0, 0.0, 1.0));
    float vg = dot(gg, f - vec3(0.0, 1.0, 1.0));
    float vh = dot(gh, f - vec3(1.0, 1.0, 1.0));
    
    vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
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
    float m = 0.0;

	for (int i = 0; i < n; i++) {
        m += a;
		v += a * worley(x, f);
        f *= 2.0;
		a *= 0.5;
	}
    
	return remap(v, 0.0, m, 0.0, 1.0);
}

float fbm_worley(vec3 x, float f, uint n) 
{
    float v = 0.0;
	float a = 0.5;
    float m = 0.0;

	for (int i = 0; i < n; i++) {
        m += a;
		v += a * worley(x, f);
        f *= 2.0;
		a *= 0.5;
	}
    
	return remap(v, 0.0, m, 0.0, 1.0);
}

float fbm_perlin(vec2 x, float f, uint n) 
{
	float v = 0.0;
	float a = 1.0;
    float m = 0.0;

	for (int i = 0; i < n; i++) {
        m += a;
		v += a * perlin(x, f);
        f *= 2.0;
		a *= 0.5;
	}

	return remap(v, 0.0, m, 0.0, 1.0);
}

float fbm_perlin(vec3 x, float f, uint n) 
{
	float v = 0.0;
	float a = 0.5;
    float m = 0.0;

	for (int i = 0; i < n; i++) {
        m += a;
		v += a * perlin(x, f);
        f *= 2.0;
		a *= 0.5;
	}

	return remap(v, 0.0, m, 0.0, 1.0);
}