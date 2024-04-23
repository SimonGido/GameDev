//#type vertex
#version 450

#include "Resources/Shaders/Includes/PBR.glsl"

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;



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


layout(std430, binding = 21) buffer buffer_Grass
{		
	vec4 GrassPosition[];
};

layout(push_constant) uniform Transform
{
	mat4 Transform;

} u_Renderer;

struct VertexOutput
{
	vec4 Color;
	vec3 Position;
	vec3 Normal;
	float Depth;
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
	int id = gl_InstanceIndex;
	vec4 position = u_Renderer.Transform * vec4(a_Position, 1.0);
	vec4 instancePosition = position + GrassPosition[id];

	mat4 viewMatrix = inverse(u_InverseView);
	mat4 viewProjection = inverse(u_InverseProjection) * viewMatrix;
	
	v_Output.Color = a_Color;
	v_Output.Position = instancePosition.xyz;
	v_Output.Normal = mat3(u_Renderer.Transform) * a_Normal;
	v_Output.Depth = -(viewMatrix * instancePosition).z;
	gl_Position = viewProjection * instancePosition;
}


#type fragment
#version 450

struct VertexOutput
{
	vec4 Color;
	vec3 Position;
	vec3 Normal;
	float Depth;
};
layout(location = 0) in VertexOutput v_Input;

layout(location = 0) out vec4 o_Color;

void main()
{
	o_Color = v_Input.Color;
}
