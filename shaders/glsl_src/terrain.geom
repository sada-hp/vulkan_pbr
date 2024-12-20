#version 460

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec4 inPositions[];
layout(location = 1) in vec3 inNormals[];
layout(location = 2) in float inHeights[];

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out float outHeight;

 void main() 
 {
	 vec3 v0v1 = inPositions[2].xyz - inPositions[0].xyz;
	 vec3 v0v2 = inPositions[1].xyz - inPositions[0].xyz;
	 vec3 normal = normalize(cross( v0v2, v0v1 ));

	 for( int vertex = 0; vertex < 3; ++vertex ) 
	 {
		 gl_Position = gl_in[vertex].gl_Position;
         
		 outNormal = normal;
		 outPosition = inPositions[vertex];
         outHeight = inHeights[vertex];

		 EmitVertex();
	 }
	 EndPrimitive();
 }
