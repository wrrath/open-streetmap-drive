#version 450

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    float centerLine = smoothstep(0.48, 0.50, abs(fract(fragUv.y * 0.08) - 0.5));
    vec3 asphalt = vec3(0.10, 0.105, 0.10);
    vec3 paint = vec3(0.85, 0.78, 0.42);
    outColor = vec4(mix(paint, asphalt, centerLine), 1.0);
}
