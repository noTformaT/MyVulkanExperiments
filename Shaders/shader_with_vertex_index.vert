#version 450

layout (location = 0) out vec3 fragColor; // out color for vertex (location is required)

// Triangle vertex position
vec3 positions[3] = vec3[] (
    vec3(0.0f, -0.4f, 0.0f),
    vec3(0.4f, 0.4f, 0.0f),
    vec3(-0.4f, 0.4f, 0.0f)
);

// Triangle vertex color
vec3 colors[3] = vec3[] (
    vec3(1.0f, 0.0f, 0.0f),
    vec3(0.0f, 1.0f, 0.0f),
    vec3(0.0f, 0.0f, 1.0f)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 1.0f);
    fragColor = colors[gl_VertexIndex];
}