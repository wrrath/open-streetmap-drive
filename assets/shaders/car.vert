#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// The CPU supplies projection * view * model for each object. Keeping this
// transform in a push constant avoids descriptor setup for this tiny mesh.
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    fragColor = inColor;
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
}
