#define PI 3.14159265359
#define ONE_OVER_PI 1.0 / PI
#define ONE_OVER_2PI 1.0 / (2.0 * PI)
#define ONE_OVER_4PI 1.0 / (4.0 * PI)

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

void SphereDistances(vec3 ro, vec3 rd, vec3 so, float radius, out vec2 t)
{
    t = vec2(0.0);

	float radius2 = radius * radius;

	vec3 L = ro - so;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius2;

	float discr = b * b - 4.0 * a * c;

	if (discr < 0.0) 
        return;

    t = max(vec2((-b - sqrt(discr))/2, (-b + sqrt(discr))/2), 0.0);
}

float SphereMaxDistance(vec3 ro, vec3 rd, vec3 so, float radius)
{
    vec2 t;
    SphereDistances(ro, rd, so, radius, t);

    return max(t.x, t.y);
}

float SphereMinDistance(vec3 ro, vec3 rd, vec3 so, float radius)
{
    vec2 t;
    SphereDistances(ro, rd, so, radius, t);

    if (t.x == 0.0 && t.y > 0.0)
    {
        return t.y;
    }
    else if (t.x > 0.0 && t.y == 0.0)
    {
        return t.x;
    }
    else
    {
        return min(t.x, t.y);
    }
}

bool SphereIntersect(vec3 ro, vec3 rd, vec3 so, float radius, out vec3 p1)
{
	float t = SphereMinDistance(ro, rd, so, radius);
    p1 = ro + rd * t;

	return t > 0.0;
}

bool SphereIntersect(vec3 ro, vec3 rd, vec3 so, float radius)
{
    return SphereMinDistance(ro, rd, so, radius) > 0.0;
}

vec2 SpehereUV(vec3 normal)
{
    return vec2(0.5 + atan(normal.z, normal.x) * ONE_OVER_2PI, 1.0 - (0.5 + asin(normal.y) * ONE_OVER_PI));
}