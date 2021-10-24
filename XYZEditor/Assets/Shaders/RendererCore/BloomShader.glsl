//#type compute
#version 460


#define MODE_PREFILTER      0
#define MODE_DOWNSAMPLE     1
#define MODE_UPSAMPLE_FIRST 2
#define MODE_UPSAMPLE       3


layout(binding = 0, rgba32f) uniform image2D   o_Image;
layout(binding = 1)			 uniform sampler2D u_Texture;
layout(binding = 2)			 uniform sampler2D u_BloomTexture;

uniform float u_FilterTreshold;
uniform float u_FilterKnee;
uniform float u_LOD;
uniform int   u_Mode;

const vec3 c_Luminance = vec3(0.2126, 0.7152, 0.0722);
const float c_Epsilon = 1.0e-4;



vec3 DownsampleBox13(sampler2D tex, float lod, vec2 uv, vec2 texelSize)
{
    // Center
    vec3 A = textureLod(tex, uv, lod).rgb;

    texelSize *= 0.5f; // Sample from center of texels

    // Inner box
    vec3 B = textureLod(tex, uv + texelSize * vec2(-1.0f, -1.0f), lod).rgb;
    vec3 C = textureLod(tex, uv + texelSize * vec2(-1.0f, 1.0f), lod).rgb;
    vec3 D = textureLod(tex, uv + texelSize * vec2(1.0f, 1.0f), lod).rgb;
    vec3 E = textureLod(tex, uv + texelSize * vec2(1.0f, -1.0f), lod).rgb;

    // Outer box
    vec3 F = textureLod(tex, uv + texelSize * vec2(-2.0f, -2.0f), lod).rgb;
    vec3 G = textureLod(tex, uv + texelSize * vec2(-2.0f, 0.0f), lod).rgb;
    vec3 H = textureLod(tex, uv + texelSize * vec2(0.0f, 2.0f), lod).rgb;
    vec3 I = textureLod(tex, uv + texelSize * vec2(2.0f, 2.0f), lod).rgb;
    vec3 J = textureLod(tex, uv + texelSize * vec2(2.0f, 2.0f), lod).rgb;
    vec3 K = textureLod(tex, uv + texelSize * vec2(2.0f, 0.0f), lod).rgb;
    vec3 L = textureLod(tex, uv + texelSize * vec2(-2.0f, -2.0f), lod).rgb;
    vec3 M = textureLod(tex, uv + texelSize * vec2(0.0f, -2.0f), lod).rgb;

    // Weights
    vec3 result = vec3(0.0);
    // Inner box
    result += (B + C + D + E) * 0.5f;
    // Bottom-left box
    result += (F + G + A + M) * 0.125f;
    // Top-left box
    result += (G + H + I + A) * 0.125f;
    // Top-right box
    result += (A + I + J + K) * 0.125f;
    // Bottom-right box
    result += (M + A + K + L) * 0.125f;

    // 4 samples each
    result *= 0.25f;

    return result;
}
vec3 UpsampleTent9(sampler2D tex, float lod, vec2 uv, vec2 texelSize, float radius)
{
    vec4 offset = texelSize.xyxy * vec4(1.0f, 1.0f, -1.0f, 0.0f) * radius;

    // Center
    vec3 result = textureLod(tex, uv, lod).rgb * 4.0f;

    result += textureLod(tex, uv - offset.xy, lod).rgb;
    result += textureLod(tex, uv - offset.wy, lod).rgb * 2.0;
    result += textureLod(tex, uv - offset.zy, lod).rgb;

    result += textureLod(tex, uv + offset.zw, lod).rgb * 2.0;
    result += textureLod(tex, uv + offset.xw, lod).rgb * 2.0;

    result += textureLod(tex, uv + offset.zy, lod).rgb;
    result += textureLod(tex, uv + offset.wy, lod).rgb * 2.0;
    result += textureLod(tex, uv + offset.xy, lod).rgb;

    return result * (1.0f / 16.0f);
}

// Quadratic color thresholding
// curve = (threshold - knee, knee * 2, 0.25 / knee)
vec4 QuadraticThreshold(vec4 color, float threshold, vec3 curve)
{
    // Maximum pixel brightness
    float brightness = max(max(color.r, color.g), color.b);
    // Quadratic curve
    float rq = clamp(brightness - curve.x, 0.0, curve.y);
    rq = (rq * rq) * curve.z;
    color *= max(rq, brightness - threshold) / max(brightness, c_Epsilon);
    return color;
}



vec4 Prefilter(vec4 color, vec2 uv)
{
    float clampValue = 20.0f;
    vec3 curve = vec3(u_FilterTreshold - u_FilterKnee, u_FilterKnee * 2.0, 0.25 / u_FilterKnee);
    color = min(vec4(clampValue), color);
    color = QuadraticThreshold(color, u_FilterTreshold, curve);
    return color;
}


layout(local_size_x = 4, local_size_y = 4) in;
void main()
{
    vec2 imgSize = vec2(imageSize(o_Image));

    ivec2 invocID = ivec2(gl_GlobalInvocationID);
    vec2 texCoords = vec2(float(invocID.x) / imgSize.x, float(invocID.y) / imgSize.y);
    texCoords += (1.0f / imgSize) * 0.5f;

    vec2 texSize = vec2(textureSize(u_Texture, int(u_LOD)));
    vec4 color = vec4(1, 0, 1, 1);
    if (u_Mode == MODE_PREFILTER)
    {
        color.rgb = DownsampleBox13(u_Texture, u_LOD, texCoords, 1.0f / texSize);
        color = Prefilter(color, texCoords);
        color.a = 1.0f;
    }
    else if (u_Mode == MODE_DOWNSAMPLE)
    {
        // Downsample
        color.rgb = DownsampleBox13(u_Texture, u_LOD, texCoords, 1.0f / texSize);
    }
    else if (u_Mode == MODE_UPSAMPLE_FIRST)
    {
        vec2 bloomTexSize = vec2(textureSize(u_Texture, int(u_LOD + 1.0f)));
        float sampleScale = 1.0f;
        vec3 upsampledTexture = UpsampleTent9(u_Texture, u_LOD + 1.0f, texCoords, 1.0f / bloomTexSize, sampleScale);

        vec3 existing = textureLod(u_Texture, texCoords, u_LOD).rgb;
        color.rgb = existing + upsampledTexture;
    }
    else if (u_Mode == MODE_UPSAMPLE)
    {
        vec2 bloomTexSize = vec2(textureSize(u_BloomTexture, int(u_LOD + 1.0f)));
        float sampleScale = 1.0f;
        vec3 upsampledTexture = UpsampleTent9(u_BloomTexture, u_LOD + 1.0f, texCoords, 1.0f / bloomTexSize, sampleScale);

        vec3 existing = textureLod(u_Texture, texCoords, u_LOD).rgb;
        color.rgb = existing + upsampledTexture;
    }
    imageStore(o_Image, ivec2(gl_GlobalInvocationID), color);
}
