#include "common.glsl"
#include "noise.glsl"

// Practical Real-Time Hex-Tiling
// Morten S. Mikkelsen
// Journal of Computer Graphics Techniques Vol. 11, No. 2, 2022

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

void hex2colTex(sampler2D col, sampler2D arm, vec2 st, out SMaterial Material, out vec3 weights)
{
    vec2 dSTdx = dFdx(st), dSTdy = dFdy(st);

    float w1, w2, w3;
    ivec2 v1, v2, v3;
    
    TrangleGrid(st, w1, w2, w3, v1, v2, v3);

    vec2 st1 = st + noise2(v1);
    vec2 st2 = st + noise2(v2);
    vec2 st3 = st + noise2(v3);

    vec4 c1 = textureGrad(col, st1, dSTdx, dSTdy);
    vec4 c2 = textureGrad(col, st2, dSTdx, dSTdy);
    vec4 c3 = textureGrad(col, st3, dSTdx, dSTdy);

    vec4 a1 = textureGrad(arm, st1, dSTdx, dSTdy);
    vec4 a2 = textureGrad(arm, st2, dSTdx, dSTdy);
    vec4 a3 = textureGrad(arm, st3, dSTdx, dSTdy);

    vec3 Lw = vec3(0.299, 0.587, 0.114);
    vec3 Dw = vec3(dot(c1.xyz, Lw), dot(c2.xyz, Lw), dot(c3.xyz, Lw));

    Dw = mix(vec3(1.0), Dw, vec3(0.6));
    vec3 W = Dw * pow(vec3(w1, w2, w3), vec3(7));

    W /= (W.x + W.y + W.z);

    Material.Albedo = W.x * c1 + W.y * c2 + W.z * c3;
    Material.AO = W.x * a1.r + W.y * a2.r + W.z * a3.r;
    Material.Roughness = W.x * a1.g + W.y * a2.g + W.z * a3.g;
    Material.Metallic = W.x * a1.b + W.y * a2.b + W.z * a3.b;
    Material.Specular = W.x * a1.a + W.y * a2.a + W.z * a3.a;
    weights = ProduceHexWeights(W.xyz, v1, v2, v3);
}