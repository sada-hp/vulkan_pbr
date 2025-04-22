#ifndef _COMMON_SHADER
#define _COMMON_SHADER

#define PI 3.1415926535897932384626433832795
#define TWO_PI 2 * PI
#define ONE_OVER_PI 1.0 / PI
#define ONE_OVER_2PI 1.0 / (2.0 * PI)
#define ONE_OVER_4PI 1.0 / (4.0 * PI)

float smootherstep(float e0, float e1, float x)
{
	x = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
	return x * x * x * (x * (6.0 * x - 15.0) + 10.0);
}

float inverse_smoothstep(float e0, float e1, float x)
{
	x = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
	return 0.5 - sin(asin(1.0 - 2.0 * x) / 3.0);
}

float ridge_smoothstep(float e0, float e1, float x)
{
	x = clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    float sm = x * x * (3 - 2 * x);
    float inv = 0.5 - sin(asin(1.0 - 2.0 * x) / 3.0);

    return sm * inv;
}

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + (((orig - old_min) / (old_max - old_min)) * (new_max - new_min));
}

float saturate(float x)
{
    return clamp(x, 0.0, 1.0);
}

vec2 saturate(vec2 x)
{
    return clamp(x, 0.0, 1.0);
}

vec3 saturate(vec3 x)
{
    return clamp(x, 0.0, 1.0);
}

vec4 saturate(vec4 x)
{
    return clamp(x, 0.0, 1.0);
}

float saturateAngle(float x)
{
    return clamp(-1.0, 1.0, x);
}

vec4 SampleOnSphere(vec3 Position, sampler2D Target, float Scale)
{
    float l = length(Position);
    vec3 n = Position / l;
    float pole = abs(dot(n, vec3(0, 1, 0)));
    vec2 uv1 = 0.5 + vec2(atan(n.z, n.x) * ONE_OVER_2PI, asin(n.y) * ONE_OVER_2PI);
    vec2 uv2 = 0.5 + vec2(atan(n.x, n.y) * ONE_OVER_2PI, asin(n.z) * ONE_OVER_2PI);

    float left = pole > 0.9 ? 0.0 : (pole >= 0.8 ? 1.0 - ((pole - 0.8) / (0.9 - 0.8)) : 1.0);
    float right = 1.0 - left;

    vec4 Out = vec4(0.0);
    if (left > 0.0)
        Out += left * texture(Target, uv1 * l * Scale);

    if (right > 0.0)
        Out += right * texture(Target, uv2 * l * Scale);

    return Out;
}

vec4 SampleProject(vec3 Position, sampler2D Target, float Scale)
{
    Position *= Scale;
    vec4 s1 = texture(Target, Position.xz);
    vec4 s2 = texture(Target, Position.xy);
    vec4 s3 = texture(Target, Position.yz);

    vec3 n = normalize(Position);
    float a1 = abs(dot(n, vec3(0, 1, 0)));
    float a2 = abs(dot(n, vec3(0, 0, 1)));
    float a3 = abs(dot(n, vec3(1, 0, 0)));

    return (s1 * a1 + s2 * a2 + s3 * a3) / (a1 + a2 + a3);
}

vec2 SphereDistances(vec3 ro, vec3 rd, vec3 so, float radius)
{
    vec2 t = vec2(0.0);

	float radius2 = radius * radius;

	vec3 L = ro - so;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L, L) - radius2;

	float discr = b * b - 4.0 * a * c;

	if (discr < 0.0) 
        return t;

    t = max(vec2((-b - sqrt(discr))/2, (-b + sqrt(discr))/2), 0.0);

    return t;
}

float SphereMaxDistance(vec3 ro, vec3 rd, vec3 so, float radius)
{
    vec2 t = SphereDistances(ro, rd, so, radius);

    return max(t.x, t.y);
}

float SphereMinDistance(vec3 ro, vec3 rd, vec3 so, float radius)
{
    vec2 t = SphereDistances(ro, rd, so, radius);

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

float RoundToIncrement(float value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

vec2 RoundToIncrement(vec2 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

vec3 RoundToIncrement(vec3 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

dvec3 RoundToIncrement(dvec3 value, float increment)
{
    return round(value * (1.0 / increment)) * increment;
}

float GetScatteringFactor(float CloudCoverage)
{
    return saturate(0.1 + smoothstep(0.5, 1.0, saturate(1.0 - pow(CloudCoverage, 5.0) * 5.0)));
}

#ifdef _UBO_SHADER
vec3 GetWorldPosition(vec2 UV, float Depth)
{
    vec4 ClipSpace     = vec4(2.0 * UV - 1.0, Depth, 1.0);
    vec4 WorldPosition = vec4(ubo.ViewMatrixInverse * (ubo.ProjectionMatrixInverse * ClipSpace));

    return WorldPosition.xyz / WorldPosition.w;
}

vec4 GetScreenPosition(vec4 World)
{
    World = vec4(ubo.ViewProjectionMatrix * World);
    World.xyz = (World.xyz / World.w);
    World.xy = World.xy * 0.5 + 0.5;
    World.y = 1.0 - World.y;

    return World;
}

dmat4 GetWorldMatrix(mat3x4 Orientation, dvec3 Offset)
{
    return dmat4(Orientation[0][0], Orientation[0][1], Orientation[0][2], 0.0,
                 Orientation[1][0], Orientation[1][1], Orientation[1][2], 0.0,
                 Orientation[2][0], Orientation[2][1], Orientation[2][2], 0.0,
                 Offset[0], Offset[1], Offset[2], 1.0);
}

dmat3 GetTerrainOrientation()
{
    dmat3 Orientation;
    Orientation[0] = vec3(1.0, 0.0, 0.0);
    Orientation[1] = normalize(floor(ubo.CameraPositionFP64.xyz));
    Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
    Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));

    return Orientation;
}

dmat3 GetTerrainOrientation(dvec3 U)
{
    dmat3 Orientation;
    Orientation[0] = vec3(1.0, 0.0, 0.0);
    Orientation[1] = U;
    Orientation[2] = normalize(cross(Orientation[0], Orientation[1]));
    Orientation[0] = normalize(cross(Orientation[1], Orientation[2]));

    return Orientation;
}
#endif

#endif