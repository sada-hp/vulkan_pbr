//Hash Functions for GPU Rendering (http://www.jcgt.org/published/0009/03/02/)
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
    vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yzx+33.33);

    return fract((p3.xx+p3.yz)*p3.zy);
}

vec3 noise3(vec3 p)
{
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz+33.33);
    return fract((p.xxy + p.yxx)*p.zyx);
}

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + ((orig - old_min) / (old_max - old_min)) * (new_max - new_min);
}

float worley(vec2 p0, float freq)
{
    float min_d = 1.0;
    const vec2 i = floor(p0);
    const vec2 f = fract(p0);

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
    const vec3 i = floor(p0);
    const vec3 f = fract(p0);

    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            for (int z = -1; z <= 1; z++) {
                vec3 p1 = vec3(x, y, z);
                vec3 neigbour_cell = mod(i + p1, freq);
                min_d = min(min_d, distance(noise3(neigbour_cell), f - p1));
            }
        }
    }

    return 1.0 - min_d;
}

float perlin(vec2 x0, float freq) 
{
    vec2 i = floor(x0);
    vec2 f = fract(x0);

	float va = noise(i);
    float vb = noise(mod(i + vec2(1.0, 0.0), freq));
    float vc = noise(mod(i + vec2(0.0, 1.0), freq));
    float vd = noise(mod(i + vec2(1.0, 1.0), freq));

    vec2 u = smoothstep(0.0, 1.0, f);
    return mix(va, vb, u.x) + (vc - va) * u.y * (1.0 - u.x) + (vd - vb) * u.x * u.y;
}

float perlin(vec3 x0, float freq) 
{
    vec3 i = floor(x0);
    vec3 f = fract(x0);
    
    float va = noise(mod(i + vec3(0., 0., 0.), freq));
    float vb = noise(mod(i + vec3(1., 0., 0.), freq));
    float vc = noise(mod(i + vec3(0., 1., 0.), freq));
    float vd = noise(mod(i + vec3(1., 1., 0.), freq));
    float ve = noise(mod(i + vec3(0., 0., 1.), freq));
    float vf = noise(mod(i + vec3(1., 0., 1.), freq));
    float vg = noise(mod(i + vec3(0., 1., 1.), freq));
    float vh = noise(mod(i + vec3(1., 1., 1.), freq));
	
    vec3 u = f * f * f * (f * (f * 6. - 15.) + 10.);
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
	for (int i = 0; i < n; i++) {
		v += a * worley(x, f);
		x *= 2.0;
        f *= 2.0;
		a *= 0.5;
	}
    
	return v;
}

float fbm_worley(vec3 x, float f, uint n) 
{
    float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < n; i++) {
		v += a * worley(x, f);
		x *= 2.0;
        f *= 2.0;
		a *= 0.5;
	}
    
	return v;
}

float fbm_perlin(vec2 x, float f, uint n) 
{
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
		x *= 2.0;
        f *= 2.0;
		a *= 0.5;
	}

	return v;
}

float fbm_perlin(vec3 x, float f, uint n) 
{
	float v = 0.0;
	float a = 0.5;
	for (int i = 0; i < n; i++) {
		v += a * perlin(x, f);
		x *= 2.0;
        f *= 2.0;
		a *= 0.5;
	}

	return v;
}