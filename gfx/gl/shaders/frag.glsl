#version 330 core
out vec4 FragColor;

//uniform sampler2D texture1;
uniform vec4 bg_color;

in vec2 out_uv;
in vec4 out_color;

void main() {
    //float GlyphShape = texture(texture1, out_uv).r;
    //if (GlyphShape == 0) discard;
    //vec4 SampledColor = GlyphShape*out_color;
    vec4 SampledColor = out_color;
    //if (SampledColor.a != 0)
        FragColor = vec4(mix(bg_color.rgb, SampledColor.rgb, SampledColor.a), max(bg_color.a, SampledColor.a));
}
