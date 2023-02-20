#version 330 core
layout (location = 0) in vec2 POS;
layout (location = 1) in vec4 COLOR;

out vec4 out_color;

void main() {
    out_color = COLOR;
    gl_Position = vec4(POS, 0.0, 1.0);
}
