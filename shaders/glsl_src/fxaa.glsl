vec3 fxaa_apply(sampler2D frame, vec2 inUV, vec2 frameBufSize)
{
    float FXAA_SPAN_MAX = 8.0;
    float FXAA_REDUCE_MUL = 0.125f;
    float FXAA_REDUCE_MIN = 0.0078125f; //1.0/128.0

    vec3 rgbNW = texelFetch(frame, ivec2(gl_FragCoord.xy + vec2(-1.0,-1.0)), 0).xyz;
    vec3 rgbNE = texelFetch(frame, ivec2(gl_FragCoord.xy + vec2(1.0,-1.0)), 0).xyz;
    vec3 rgbSW = texelFetch(frame, ivec2(gl_FragCoord.xy + vec2(-1.0,1.0)), 0).xyz;
    vec3 rgbSE = texelFetch(frame, ivec2(gl_FragCoord.xy + vec2(1.0,1.0)), 0).xyz;
    vec3 rgbM = texelFetch(frame, ivec2(gl_FragCoord.xy), 0).xyz;

    vec3 luma=vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max
    (
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN
    );

    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) / frameBufSize;

    vec3 rgbA = (1.0/2.0) * (
        texture(frame, inUV.xy + dir * (1.0/3.0 - 0.5)).xyz +
        texture(frame, inUV.xy + dir * (2.0/3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture(frame, inUV.xy + dir * (0.0/3.0 - 0.5)).xyz +
        texture(frame, inUV.xy + dir * (3.0/3.0 - 0.5)).xyz);
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax))
    {
        return rgbA;
    }
    else
    {
        return rgbB;
    }
}