#version 330 core
out vec4 FragColor;

uniform sampler2D texture1;
uniform vec4 bg_color;

in vec2 out_uv;
in vec4 out_color;

void main() {
    float GlyphShape = texture(texture1, out_uv).r;
    if (GlyphShape == 0) discard;
    vec4 SampledColor = GlyphShape*out_color;
    if (SampledColor.a <= 0) discard;
    SampledColor.a = mix(SampledColor.a, 1, SampledColor.a);
    if (SampledColor.a >= 0.9) SampledColor.a = 1;
    FragColor = SampledColor;
}
