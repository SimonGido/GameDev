#type vertex
#version 450 core

#include "Resources/Shaders/Includes/PBR.glsl"

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_TexCoord;


struct VertexOutput
{
	mat4 InverseProjectionMatrix;
	vec2 TexCoord;
	vec2 ViewportSize;
};

layout (std140, binding = 16) uniform Scene
{
	// Camera info
	mat4 u_InverseProjection;
	mat4 u_InverseView;	
	vec4 u_CameraPosition;
	vec4 u_ViewportSize;

	// Light info
	DirectionalLight u_DirectionalLight;
};

layout(push_constant) uniform Transform
{
	mat4 Transform;
} u_Renderer;

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.TexCoord = a_TexCoord;
	v_Output.InverseProjectionMatrix = u_InverseProjection;
    gl_Position = u_Renderer.Transform * vec4(a_Position.xy, 0.0, 1.0);
}


#type fragment
#version 450

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	mat4 InverseProjectionMatrix;
	vec2 TexCoord;
};

layout(location = 0) in VertexOutput v_Input;

layout(binding = 0) uniform sampler2D u_RasterImage;
layout(binding = 1) uniform sampler2D u_RasterDepthImage;

layout(binding = 2, rgba32f) uniform image2D o_Image;
layout(binding = 3, rgba32f) uniform image2D o_DepthImage;


const float FarClip = 2000.0;
const float NearClip = 0.1;

float DistToDepth(float dist) 
{
    // Initialize world position with the negative distance
    vec4 worldPos;
    worldPos.x = v_Input.TexCoord.x * 2.0 - 1.0;
    worldPos.y = -(v_Input.TexCoord.y * 2.0 - 1.0);
    worldPos.z = -dist;
    worldPos.w = 1.0;

    // Apply the projection matrix to transform worldPos to clip space
    vec4 clipPos = inverse(v_Input.InverseProjectionMatrix) * worldPos;

    // Perform perspective division
    vec4 ndcPos = clipPos / clipPos.w;

    // The z-component of ndcPos is the depth value in screen space
    float depth = ndcPos.z;

    return depth;
}

float DepthToDist(float depth) 
{
	vec4 screenPos;
	screenPos.x = v_Input.TexCoord.x * 2.0 - 1.0;
	screenPos.y = -(v_Input.TexCoord.y * 2.0 - 1.0);
	screenPos.z = depth;
	screenPos.w = 1.0; 
	
	vec4 worldPos = v_Input.InverseProjectionMatrix * screenPos;
	worldPos /= worldPos.w;
	return -worldPos.z;
}



float LinearizeDepth(float depthBufferValue, float near, float far) 
{
    return (2.0 * near * far) / (far + near - depthBufferValue * (far - near));
}


void main()
{    
    vec4 rasterColor	= texture(u_RasterImage, v_Input.TexCoord);
	float depth			= texture(u_RasterDepthImage, v_Input.TexCoord).r;
	
	float linearDepth	= LinearizeDepth(depth, NearClip, FarClip) / FarClip;
	float dist			= DepthToDist(depth);
	float calcLinearDepth = LinearizeDepth(DistToDepth(dist), NearClip, FarClip) / FarClip;

	ivec2 pixel = ivec2(gl_FragCoord.xy);
	//imageStore(o_Image, pixel, vec4(dist, dist, dist, 1.0));
	//imageStore(o_Image, pixel, vec4(linearDepth, linearDepth, linearDepth, 1.0));
	//imageStore(o_Image, pixel, vec4(calcLinearDepth, calcLinearDepth, calcLinearDepth, 1.0));


	imageStore(o_Image, pixel, rasterColor);
	imageStore(o_DepthImage, pixel, vec4(dist, 0, 0, 1));
}