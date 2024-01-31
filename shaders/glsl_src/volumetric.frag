#version 460
#include "ubo.glsl"

layout(location = 0) in vec3 Position;

layout(location=0) out vec4 outColor;

layout(binding = 2) uniform sampler3D CloudLowFrequency;

struct Box
{
    vec3 max;
    vec3 min;
};

bool raySphereintersection(vec3 ro, vec3 rd, float radius, out vec3 p1, out vec3 p2)
{
	vec3 sphereCenter = vec3(0.0);

	float radius2 = radius*radius;

	vec3 L = ro - sphereCenter;
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, L);
	float c = dot(L,L) - radius2;

	float discr = b*b - 4.0 * a * c;
	if(discr < 0.0) return false;
	float t1 = max(0.0, (-b + sqrt(discr))/2);
    float t2 = max(0.0, (-b - sqrt(discr))/2);
	if(t1 == 0.0 || t2 == 0.0){
		return false;
	}

    p1 = ro + rd * t1;
    p2 = ro + rd * t2;

	return true;
}

float remap(float orig, float old_min, float old_max, float new_min, float new_max)
{
    return new_min + ((orig - old_min) / (old_max - old_min)) * (new_max - new_min);
}

float SampleDensity(vec3 x0)
{
    vec4 low_frequency_noise = texture(CloudLowFrequency, x0);
    float low_frequency_fbm = low_frequency_noise.g * 0.625 + low_frequency_noise.b * 0.25 + low_frequency_noise.a * 0.125;
    float density = remap(low_frequency_noise.r, (1.0 - low_frequency_fbm), 1.0, 0.0, 1.0);

    return density;
}

vec4 RayMarch(vec3 rs, vec3 re, vec3 d)
{
    const int steps = 1000;
    const float stepsize = distance(rs, re) / float(steps);
    vec3 cloud_scatter = vec3(0, 0, 0);
    float transparency = 1.0;

    vec3 pos = rs;

    for (int i = 0; i < steps; i++) {
        pos += stepsize * d;

        float density = SampleDensity(pos / 500.0);
        if (density > 0.0) {
            float transmittance_cloud = exp(-stepsize * density * 0.01);
            float scatter_amount = 0.7;
            cloud_scatter += transparency * (1.0 - transmittance_cloud) * scatter_amount;
            transparency *= transmittance_cloud;
        }
    }

    return clamp(vec4(cloud_scatter, 1.0), 0.0, 1.0);
}

void main()
{
    vec2 ScreenUV = gl_FragCoord.xy / View.Resolution;
    vec4 ScreenNDC = vec4(2.0 * ScreenUV - 1.0, 1.0, 1.0);
    vec4 ScreenView = inverse(ubo.ProjectionMatrix) * ScreenNDC;
    vec4 ScreenWorld = inverse(ubo.ViewMatrix) * vec4(ScreenView.xy, 1.0, 0.0);
    vec3 RayDirection = normalize(ScreenWorld.xyz);

#if 0
    float distance = 0.0;
    if (raySphereintersection(View.CameraPosition.xyz, RayDirection, 50.0, distance)) {
        outColor = vec4(1.0 - exp(-distance * 0.0025));
    }
    else {
        outColor = vec4(0.0);
    }
#else
    vec3 sphereStart;
    vec3 sphereEnd;

    if (raySphereintersection(View.CameraPosition.xyz, RayDirection, 50.0, sphereStart, sphereEnd)
    && raySphereintersection(View.CameraPosition.xyz, RayDirection, 50.0, sphereStart, sphereEnd)) {
        outColor = RayMarch(sphereStart, sphereEnd, RayDirection);
    }
    else {
        outColor = vec4(0.0);
    }
#endif
}