//#type vertex
#version 450 core

layout(location = 0) in vec3  a_Position;
layout(location = 1) in vec2  a_TexCoord;


struct VertexOutput
{
	vec2 TexCoord;
};

layout(push_constant) uniform Transform
{
	mat4 Transform;
} u_Renderer;

layout(location = 0) out VertexOutput v_Output;

void main()
{
    v_Output.TexCoord = a_TexCoord;
    gl_Position = u_Renderer.Transform * vec4(a_Position.xy, 0.0, 1.0);
}


#type fragment
#version 450

layout(location = 0) out vec4 o_Color;

struct VertexOutput
{
	vec2 TexCoord;
};

layout(location = 0) in VertexOutput v_Input;

layout(binding = 0) uniform sampler2D u_ColorTexture;


void main()
{    
    vec4 color = texture(u_ColorTexture, v_Input.TexCoord);
	o_Color = color;
}