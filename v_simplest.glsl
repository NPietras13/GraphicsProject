#version 330 core

in vec4 vertex;
in vec2 texCoord;
out vec2 TexCoord;

uniform mat4 P;
uniform mat4 V;
uniform mat4 M;

void main() {
    TexCoord = texCoord;
    gl_Position = P * V * M * vertex;
}
