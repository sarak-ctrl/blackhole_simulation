#version 410 core
in vec2 vUV;
out vec4 fragColor;

uniform float uTime;
uniform float uMass;
uniform float uSpin;
uniform float uDiskBright;
uniform float uAspect;
uniform vec3  uCamPos;
uniform float uCamTheta;
uniform float uCamPhi;
uniform float uCamDist;

void main() {
    vec2 uv = vUV * 2.0 - 1.0;
    uv.x *= uAspect;

    vec3 ro = uCamPos;
    vec3 target = vec3(0.0);
    vec3 forward = normalize(target - ro);
    vec3 right = normalize(cross(forward, vec3(0,1,0)));
    vec3 up = cross(right, forward);
    vec3 rd = normalize(forward + uv.x*right*0.6 + uv.y*up*0.6);

    float M = uMass * 2.0;
    vec3 pos = ro;
    vec3 vel = rd;
    vec3 color = vec3(0.0);
    float transmit = 1.0;

    for(int i = 0; i < 200; i++) {
        float r = length(pos);

        if(r < M) {
            color = vec3(0.0);
            transmit = 0.0;
            break;
        }

        if(r > 100.0) {
            // stars + faint nebula floor so the background is never pure black
            float h = fract(sin(dot(normalize(vel), vec3(127.1,311.7,74.7)))*43758.5);
            color += transmit * (vec3(0.01, 0.015, 0.03) + vec3(h * 0.4) * 0.35);
            break;
        }

        // gravity
        float r2 = dot(pos,pos);
        float r3 = r2*r;
        vec3 acc = -1.5*M/r3 * pos;
        float h = 0.05 * clamp(r/5.0, 0.1, 1.0);
        vel += acc * h;
        pos += vel * h;

        // disk
        float diskR = length(pos.xz);
        float rISCO = M * 3.0;
        float rOut  = rISCO * 2.8;

        if(diskR > rISCO && diskR < rOut && abs(pos.y) < 0.6) {
            float dens = (1.0 - abs(pos.y)/0.6);
            dens *= smoothstep(rISCO, rISCO*1.2, diskR);
            // Defined fade-out at disk outer edge (avoid reversed smoothstep args).
            dens *= (1.0 - smoothstep(rOut * 0.8, rOut, diskR));

            float temp = 8000.0 * uDiskBright * pow(rISCO/diskR, 0.75);
            temp = clamp(temp, 1000.0, 20000.0);

            vec3 dcol;
            float t = temp/20000.0;
            dcol = mix(vec3(1.0,0.2,0.0), vec3(1.0,0.8,0.3), t);
            dcol = mix(dcol, vec3(1.0,1.0,1.0), max(0.0, t-0.5)*2.0);

            // doppler
            vec3 tang = normalize(cross(vec3(0,1,0), pos));
            float beta = 0.4 * sqrt(M/max(diskR,0.1));
            float dop = 1.0 + beta * dot(tang, -normalize(vel));
            dop = clamp(dop, 0.1, 5.0);

            // shimmer
            dcol *= 1.0 + 0.15*sin(diskR*3.0 - uTime*4.0);
            dcol *= dens * dop * uDiskBright * 1.6;

            float alpha = clamp(dens * 0.65, 0.0, 1.0);
            color    += transmit * dcol * alpha;
            transmit *= (1.0 - alpha * 0.6);
            if(transmit < 0.01) break;
        }
    }

    // Kept this pass in HDR; final tone mapping happens in composite.frag.
    fragColor = vec4(color, 1.0);
}