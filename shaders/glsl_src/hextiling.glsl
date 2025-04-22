#include "common.glsl"
#include "noise.glsl"

// Practical Real-Time Hex-Tiling
// Morten S. Mikkelsen
// Journal of Computer Graphics Techniques Vol. 11, No. 2, 2022

struct HexParams
{
    vec2 st1, st2, st3;
    ivec2 v1, v2, v3;
    float w1, w2, w3;
    float contrast;
#ifndef HEX_COMPUTE
    vec2 dSTdx, dSTdy;
#endif
};

void TrangleGrid(in vec2 st, 
                out float w1, out float w2, out float w3,
                out ivec2 v1, out ivec2 v2, out ivec2 v3)
{
    st *= 2.0 * sqrt(3.0);

    const mat2 gridToSkewGrid = mat2(1.0, 0.0, -0.57735027, 1.15470054);
    vec2 skewedCoord = gridToSkewGrid * st;

    ivec2 baseId = ivec2(floor(skewedCoord));
    vec3 temp = vec3(fract(skewedCoord), 0);
    temp.z = 1.0 - temp.x - temp.y;

    float s = step(0.0, -temp.z);
    float s2 = 2.0 * s - 1.0;

    w1 = -temp.z * s2;
    w2 = s - temp.y * s2;
    w3 = s - temp.x * s2;

    v1 = baseId + ivec2(s, s);
    v2 = baseId + ivec2(s, 1.0 - s);
    v3 = baseId + ivec2(1.0 - s, s);
}

vec3 ProduceHexWeights(vec3 W, ivec2 vertex1, ivec2 vertex2, ivec2 vertex3)
{
    vec3 res;

    int v1 = (vertex1.x - vertex1.y) % 3;
    if (v1 < 0) v1 += 3;

    int vh = v1 < 2 ? (v1 + 1) : 0;
    int vl = v1 > 0 ? (v1 - 1) : 2;
    int v2 = vertex1.x < vertex3.x ? vl : vh;
    int v3 = vertex1.x < vertex3.x ? vh : vl;

    res.x = v3 == 0 ? W.z : (v2 == 0 ? W.y : W.x);
    res.y = v3 == 1 ? W.z : (v2 == 1 ? W.y : W.x);
    res.z = v3 == 2 ? W.z : (v2 == 2 ? W.y : W.x);

    return res;
}

void GetHexParams(float contrast, in vec2 st, out HexParams params)
{
#ifndef HEX_COMPUTE
    params.dSTdx = dFdx(st);
    params.dSTdy = dFdy(st);
#endif
    TrangleGrid(st, params.w1, params.w2, params.w3, params.v1, params.v2, params.v3);
    
    params.st1 = st + noise2(params.v1);
    params.st2 = st + noise2(params.v2);
    params.st3 = st + noise2(params.v3);
    params.contrast = contrast;
}

vec3 Gain3(vec3 W, float r)
{
    // Increase contrast when r > 0.5 and
    // reduce contrast if less.
    float k = log(max(1.0 - r, 1e-3)) / log(0.5);
    vec3 s = 2*step(0.5, W);
    vec3 m = 2*(1 - s);
    vec3 res = 0.5*s + 0.25*m * pow(max(vec3(0.0), s + W*m), vec3(k));
    return res.xyz / (res.x+res.y+res.z);
}

#ifndef HEX_COMPUTE
void hex2colTex(sampler2DArray col, sampler2DArray nor, sampler2DArray arm, int Layer, in HexParams Params, out SMaterial Material, out vec3 Weights)
{
    vec4 c1 = textureGrad(col, vec3(Params.st1, Layer), Params.dSTdx, Params.dSTdy);
    vec4 c2 = textureGrad(col, vec3(Params.st2, Layer), Params.dSTdx, Params.dSTdy);
    vec4 c3 = textureGrad(col, vec3(Params.st3, Layer), Params.dSTdx, Params.dSTdy);

    vec4 a1 = textureGrad(arm, vec3(Params.st1, Layer), Params.dSTdx, Params.dSTdy);
    vec4 a2 = textureGrad(arm, vec3(Params.st2, Layer), Params.dSTdx, Params.dSTdy);
    vec4 a3 = textureGrad(arm, vec3(Params.st3, Layer), Params.dSTdx, Params.dSTdy);

    vec4 n1 = textureGrad(nor, vec3(Params.st1, Layer), Params.dSTdx, Params.dSTdy);
    vec4 n2 = textureGrad(nor, vec3(Params.st2, Layer), Params.dSTdx, Params.dSTdy);
    vec4 n3 = textureGrad(nor, vec3(Params.st3, Layer), Params.dSTdx, Params.dSTdy);

    vec3 Lw = vec3(0.299, 0.587, 0.114);
    vec3 Dw = vec3(dot(c1.xyz, Lw), dot(c2.xyz, Lw), dot(c3.xyz, Lw));

    Dw = mix(vec3(1.0), Dw, vec3(0.6));
    vec3 W = Dw * pow(vec3(Params.w1, Params.w2, Params.w3), vec3(7));

    W /= (W.x + W.y + W.z);
    W = Params.contrast == 0.5 ? W : Gain3(W, Params.contrast);

    Material.Albedo = W.x * c1 + W.y * c2 + W.z * c3;
    Material.AO = W.x * a1.r + W.y * a2.r + W.z * a3.r;
    Material.Roughness = W.x * a1.g + W.y * a2.g + W.z * a3.g;
    Material.Metallic = W.x * a1.b + W.y * a2.b + W.z * a3.b;
    Material.Specular = W.x * a1.a + W.y * a2.a + W.z * a3.a;
    Material.Normal = 2.0 * (W.x * n1.rgb + W.y * n2.rgb + W.z * n3.rgb) - 1.0;

    Weights = ProduceHexWeights(W, Params.v1, Params.v2, Params.v3);
}
#endif