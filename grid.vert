#version 410 core

layout(location=0) in vec3 aPos;

out vec3  vWorldPos;      
out float vDistToCenter;  
out float vWarpAmount;    

uniform mat4  uVP;   
uniform float uTime;  
uniform float uMass;  
uniform float uSpin;  

void main() {
    vec3 p = aPos;  

    float r  = length(p.xz);
    float rs = 2.0 * uMass;  

    float rClamped = max(r, rs * 1.05);
    float well     = -uMass * 2.5 / rClamped;

    well *= smoothstep(rs * 0.8, rs * 3.0, r);
    p.y  += well;

    float angle = twist + uTime * 0.04 * uSpin;
    float ca = cos(angle);
    float sa = sin(angle);
    float nx = p.x * ca - p.z * sa;
    float nz = p.x * sa + p.z * ca;
    p.x = nx;
    p.z = nz;

    float wave = 0.04 * sin(r * 1.2 - uTime * 2.5) *
                 exp(-r * 0.08);  
    p.y += wave;

    vWorldPos     = p;
    vDistToCenter = r;
    vWarpAmount   = abs(well) / (uMass * 2.5 + 0.001);

    gl_Position = uVP * vec4(p, 1.0);
}
