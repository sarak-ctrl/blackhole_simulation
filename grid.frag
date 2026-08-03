#version 410 core

in vec3  vWorldPos;      
in float vDistToCenter;  
in float vWarpAmount;    

out vec4 fragColor;

uniform float uTime;  
uniform float uMass; 

void main() {
    float r  = vDistToCenter;
    float rs = 2.0 * uMass;  

    float farFade  = 1.0 - smoothstep(10.0, 15.0, r);
    float nearFade = smoothstep(rs * 1.1, rs * 2.5, r);
    float alpha    = farFade * nearFade * 0.65;

    float t    = clamp(1.0 - (r - rs) / (rs * 6.0), 0.0, 1.0);

    vec3 cyan  = vec3(0.0, 0.85, 1.0);   
    vec3 orng  = vec3(1.0, 0.45, 0.0);   
    vec3 red   = vec3(0.9, 0.05, 0.05);  

    vec3 col;
    if (t < 0.5)
        col = mix(cyan, orng, t * 2.0);
    else
        col = mix(orng, red, (t - 0.5) * 2.0);

    float glow = 1.0 + vWarpAmount * 2.5;
    col *= glow;

    float pulse = 1.0 + 0.08 * sin(r * 3.0 - uTime * 2.0);
    col *= pulse;

    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
