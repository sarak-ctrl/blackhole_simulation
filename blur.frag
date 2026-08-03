#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uTex;        
uniform int       uMode;       
uniform int       uHorizontal; 
uniform float     uThresh;    

const float weight[5] = float[](
    0.227027,   
    0.194595,  
    0.121622,   
    0.054054,  
    0.016216    
);

void main() {
    if (uMode == 0) {
        vec3  col = texture(uTex, vUV).rgb;
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));

        if (lum > uThresh)
            fragColor = vec4(col, 1.0);
        else
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);

    } else {

        vec2 texel = 1.0 / vec2(textureSize(uTex, 0));

        vec3 result = texture(uTex, vUV).rgb * weight[0];

        if (uHorizontal == 1) {
            for (int i = 1; i < 5; i++) {
                result += texture(uTex,
                    vUV + vec2(texel.x * i, 0.0)).rgb * weight[i];
                result += texture(uTex,
                    vUV - vec2(texel.x * i, 0.0)).rgb * weight[i];
            }
        } else {
            for (int i = 1; i < 5; i++) {
                result += texture(uTex,
                    vUV + vec2(0.0, texel.y * i)).rgb * weight[i];
                result += texture(uTex,
                    vUV - vec2(0.0, texel.y * i)).rgb * weight[i];
            }
        }

        fragColor = vec4(result, 1.0);
    }
}
