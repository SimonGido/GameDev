//#type vertex
#version 430 core

#include "Resources/Shaders/Includes/PBR.glsl"

layout(location = 0) in vec4 a_Color;
layout(location = 1) in vec3 a_Position;


struct VertexOutput
{
	vec4 Color;
	vec3 Position;
};

layout(location = 0) out VertexOutput v_Output;

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

void main()
{
	v_Output.Color = a_Color;
	v_Output.Position = a_Position;
	mat4 viewProjection = inverse(u_InverseProjection) * inverse(u_InverseView);
	gl_Position = viewProjection * u_Renderer.Transform * vec4(a_Position, 1.0);
}

#type fragment
#version 430 core

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec4 Color;
	vec3 Position;
};

layout(location = 0) in VertexOutput v_Input;

void main()
{
	o_Color = v_Input.Color;
}
