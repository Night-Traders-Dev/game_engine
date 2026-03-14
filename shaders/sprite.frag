#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main() {
    // For now, just output the vertex color (no texture sampling yet)
    // Texture sampling will be added when descriptor sets are wired up
    out_color = frag_color;
}
