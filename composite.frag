#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;     
uniform sampler2D uBloom;     
uniform float     uBloomStr;  

vec3 aces(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp(
        (x * (a * x + b)) / (x * (c * x + d) + e),
        0.0, 1.0
    );
}

void main() {

    vec3 scene = texture(uScene, vUV).rgb;  // raw HDR render
    vec3 bloom = texture(uBloom, vUV).rgb;  // blurred glow

    vec3 col = scene + bloom * uBloomStr;

    col = aces(col);

    col = pow(col, vec3(1.0 / 2.2));

    vec2  uv2 = vUV * 2.0 - 1.0;  // remap to (-1, 1)
    float vig = 1.0 - dot(uv2, uv2) * 0.25;
    col *= smoothstep(0.0, 0.5, vig);

    fragColor = vec4(col, 1.0);
}
