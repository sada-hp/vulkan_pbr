#include "../lighting.glsl"

#define TRANSMITTANCE_TEXTURE_WIDTH 256
#define TRANSMITTANCE_TEXTURE_HEIGHT 64

float UVFromUnitRange(float x, float size)
{
    return 0.5 / size + x * (1.0 - 1.0 / size);
}

float UnitRangeFromUV(float u, float size)
{
    return (u - 0.5 / size) / (1.0 - 1.0 /size);
}