//Hash Functions for GPU Rendering (http://www.jcgt.org/published/0009/03/02/)
#ifndef _NOISE_SHADER
#define _NOISE_SHADER

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
    n = n * (n * n * 15731 + 789221) + int(NOISE_SEED);
    return -1.0+2.0*vec2( n & ivec2(0x0fffffff))/float(0x0fffffff);
}

vec3 noise3(vec3 p)
{
    ivec3 n = ivec3(p.x*ivec3(3,37,64) + p.y*ivec3(311,113,235) + p.z*ivec3(567,256,56));
	n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + int(NOISE_SEED);
    return -1.0+2.0*vec3( n & ivec3(0x0fffffff))/float(0x0fffffff);
}

vec2 pcg2d(vec2 p)
{
    uvec2 v = uvec2(p) * 1664525u;

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v >> 16u);

    v.x += v.y * 1664525u;
    v.y += v.x * 1664525u;

    v = v ^ (v>>16u);

    return vec2(v) / float(0x0fffffff);
}

vec3 pcg3d(vec3 p)
{
    uvec3 i = uvec3(floatBitsToUint(p)) * 1664525u + 1013904223u;

    i.x += i.y*i.z;
    i.y += i.z*i.x;
    i.z += i.x*i.y;

	i ^= i >> 16u;

    i.x += i.y*i.z;
    i.y += i.z*i.x;
    i.z += i.x*i.y;

    return fract(vec3(uintBitsToFloat(i)) / vec3(0x0fffffff));
}

float worley(vec2 p0, float freq)
{
    float min_d = 1.0;
    const vec2 i = floor(p0 * freq);
    const vec2 f = fract(p0 * freq);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 p1 = vec2(x, y);
            vec2 p2 = abs(noise2(mod(i + p1, freq)));
            vec2 diff = p2 + p1 - f;

            min_d = min(min_d, dot(diff, diff));
        }
    }

    return min_d; // squared
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
                vec3 p2 = abs(noise3(mod(i + p1, freq)));
                vec3 diff = p2 + p1 - f;

                min_d = min(min_d, dot(diff, diff));
            }
        }
    }

    return min_d; // squared
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

    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    
    return mix(mix(va, vb, u.x), mix(vc, vd, u.x), u.y);
}

void perlind(vec2 x0, float freq, out float value, out vec2 d1, out vec2 d2) 
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

    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
    vec2 du2 = 60.0 * f * (f * (f * 3.0) + 1.0);

    value = mix(mix(va, vb, u.x), mix(vc, vd, u.x), u.y);

    vec2 g = mix(mix(ga, gb, u.x), mix(gc, gd, u.x), u.y);
    d1.x = g.x + du.x * ((vb - va) + u.y * (va - vb - vc + vd));
    d1.y = g.y + du.y * ((vc - va) + u.x * (va - vb - vc + vd));
    d2 = g;
}

float perlin(vec3 x0, float freq) 
{
    vec3 i = floor(x0 * freq);
    vec3 f = fract(x0 * freq);
    
    vec2 of = vec2(0.0, 1.0);

    vec3 ga = noise3(mod(i + of.xxx, freq));
    vec3 gb = noise3(mod(i + of.yxx, freq));
    vec3 gc = noise3(mod(i + of.xyx, freq));
    vec3 gd = noise3(mod(i + of.yyx, freq));
    vec3 ge = noise3(mod(i + of.xxy, freq));
    vec3 gf = noise3(mod(i + of.yxy, freq));
    vec3 gg = noise3(mod(i + of.xyy, freq));
    vec3 gh = noise3(mod(i + of.yyy, freq));

    float va = dot(ga, f - of.xxx);
    float vb = dot(gb, f - of.yxx);
    float vc = dot(gc, f - of.xyx);
    float vd = dot(gd, f - of.yyx);
    float ve = dot(ge, f - of.xxy);
    float vf = dot(gf, f - of.yxy);
    float vg = dot(gg, f - of.xyy);
    float vh = dot(gh, f - of.yyy);
    
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
	float a = 1.0;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * sqrt(worley(x, f));
        f *= 2.0;

        as += a;
		a *= 0.5;
        NOISE_SEED++;
	}
    
	return clamp(v / as, 0.0, 1.0);
}

float fbm_worley(vec3 x, float f, uint n) 
{
    float v = 0.0;
	float a = 1.0;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * worley(x, f);
        f *= 2.0;

        as += a;
		a *= 0.5;
        NOISE_SEED++;
	}
    
	return clamp(v / as, 0.0, 1.0);
}

float fbm_perlin(vec2 x, float f, uint n) 
{
	float v = 0.0;
	float a = 1.0;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
        f *= 2.0;
        as += a;
		a *= 0.5;
        NOISE_SEED++;
	}

	return clamp(v / as, -1.0, 1.0);
}

float fbm_perlin(vec3 x, float f, uint n) 
{
	float v = 0.0;
	float a = 1.0;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
        f *= 2.0;
        as += a;
		a *= 0.5;
        NOISE_SEED++;
	}

	return clamp(v / as, -1.0, 1.0);
}

float fbm_perlin_turb(vec3 x, float f, uint n) 
{
	float v = 0.0;
	float a = 1.0;
    float as = 0.0;

	for (int i = 0; i < n; i++) {
		v += a * abs(perlin(x, f));
        f *= 2.0;
        as += a;
		a *= 0.5;
        NOISE_SEED++;
	}

	return clamp(v / as, -1.0, 1.0);
}

#endif