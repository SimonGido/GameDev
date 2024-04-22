//#type vertex
#version 450

XYZ_INSTANCED layout(location = 0) in vec3  a_IPosition;
XYZ_INSTANCED layout(location = 1) in vec4  a_IColor;

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

struct VertexOutput
{
	vec4 Color;
	vec3 Position;
};

layout(location = 0) out VertexOutput v_Output;

void main()
{
	vec4 instancePosition = u_Renderer.Transform * vec4(a_IPosition, 1.0);

	mat4 viewProjection = inverse(u_InverseProjection) * inverse(u_InverseView);
	
	v_Output.Color = a_IColor;
	v_Output.Position = instancePosition.xyz;

	gl_Position = viewProjection * instancePosition;
}


#type fragment
#version 450

struct VertexOutput
{
	vec4 Color;
	vec3 Position;
};
layout(location = 0) in VertexOutput v_Input;


layout(location = 0) out vec4 o_Color;


layout(binding = 1) uniform sampler2D u_Texture;

void main()
{
	o_Color = v_Input.Color;
}
