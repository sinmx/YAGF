#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 0, set = 0, std140) uniform ObjectData
{
  mat4 ModelMatrix;
  mat4 ViewProjectionMatrix;
};

layout(binding = 1, set = 0, std140) uniform ViewMatrices
{
mat4 tobeset;
};

layout(location = 0) in vec3 Position;
layout(location = 1) in vec3 Normal;
layout(location = 2) in vec2 Texcoord;
//layout(location = 3) in vec4 Color;
/*layout(location = 4) in vec2 SecondTexcoord;
layout(location = 5) in vec3 Tangent;
layout(location = 6) in vec3 Bitangent;

layout(location = 7) in int index0;
layout(location = 8) in float weight0;
layout(location = 9) in int index1;
layout(location = 10) in float weight1;
layout(location = 11) in int index2;
layout(location = 12) in float weight2;
layout(location = 13) in int index3;
layout(location = 14) in float weight3;*/

layout(location = 0) out vec3 nor;
/*out vec3 tangent;
out vec3 bitangent;*/
layout(location = 1) out vec2 uv;
/*out vec2 uv_bis;
out vec4 color;*/

void main()
{
//  color = Color.zyxw;
  mat4 ModelViewProjectionMatrix = ViewProjectionMatrix * ModelMatrix;
//  mat4 TransposeInverseModelView = transpose(InverseModelMatrix);// *InverseViewMatrix);
  gl_Position = ModelViewProjectionMatrix * vec4(Position.xyz, 1.);
  gl_Position.y *= -1.;
  nor = Normal;
//  nor = (TransposeInverseModelView * vec4(Normal, 0.)).xyz;
  //  tangent = (TransposeInverseModelView * vec4(Tangent, 0.)).xyz;
  //  bitangent = (TransposeInverseModelView * vec4(Bitangent, 0.)).xyz;
  uv = Texcoord;
  //  uv_bis = SecondTexcoord;
}