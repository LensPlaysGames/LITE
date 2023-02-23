#version 330 core
out vec4 FragColor;

in vec4 out_color;

void main() {;
    FragColor = out_color;
    if (FragColor.a <= 0) return;
    FragColor.a = mix(FragColor.a, 1, FragColor.a);
    if (FragColor.a >= 0.9) FragColor.a = 1;
}
