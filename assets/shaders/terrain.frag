#version 450

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 grass = vec3(0.22, 0.36, 0.12);
    vec3 soil = vec3(0.34, 0.22, 0.11);
    float variation = 0.94 + 0.06 * sin(fragUv.y * 6.28318);
    outColor = vec4(mix(grass, soil, smoothstep(0.25, 0.75, fragUv.x)) * variation, 1.0);
}
