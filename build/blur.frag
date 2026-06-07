#version 410 core
// ============================================================
//  blur.frag — Bright Extract + Gaussian Blur Shader
// ============================================================
// This shader does TWO jobs depending on uMode:
//
//  Mode 0 — Bright Extract:
//    Looks at each pixel from the scene.
//    If it's brighter than uThresh, keep it.
//    If it's dimmer, output black.
//    This isolates only the glowing parts (disk, Einstein ring)
//    so we can blur ONLY those — not the whole image.
//
//  Mode 1 — Gaussian Blur:
//    Takes the bright regions and blurs them smoothly.
//    We do this in two passes:
//      Pass A: blur horizontally (left-right)
//      Pass B: blur vertically (up-down)
//    Together they create a beautiful circular glow/bloom.
//    We repeat this 6 times (ping-pong) for a wide soft bloom.
// ============================================================

// UV coordinate from bh.vert
in vec2 vUV;

// Final pixel color output
out vec4 fragColor;

// --------------------------- Uniforms ----------------------------------
uniform sampler2D uTex;        // input texture to process
uniform int       uMode;       // 0 = bright extract, 1 = blur
uniform int       uHorizontal; // 1 = blur horizontally, 0 = vertically
uniform float     uThresh;     // brightness threshold for extraction

// -------------------------- Gaussian blur weights (9-tap) --------------------------
// These weights come from a Gaussian (bell curve) distribution.
// Center pixel gets most weight (0.227), outer pixels get less.
// Using 5 weights because we sample symmetrically on both sides.
// Total = 0.227 + 2*(0.194 + 0.121 + 0.054 + 0.016) = ~1.0
const float weight[5] = float[](
    0.227027,   // center pixel
    0.194595,   // 1 pixel away
    0.121622,   // 2 pixels away
    0.054054,   // 3 pixels away
    0.016216    // 4 pixels away
);

void main() {
    if (uMode == 0) {
        // ════════════════════════════════════════════════════
        //  MODE 0: BRIGHT EXTRACT
        //  Only keep pixels brighter than the threshold.
        //  We measure brightness using luminance formula:
        //  L = 0.2126*R + 0.7152*G + 0.0722*B
        //  (matches human eye sensitivity to each color)
        // ════════════════════════════════════════════════════
        vec3  col = texture(uTex, vUV).rgb;
        // Calculate perceptual brightness of this pixel
        float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));

        if (lum > uThresh)
            // Bright enough — keep it for bloom
            fragColor = vec4(col, 1.0);
        else
            // Too dim — output black (won't contribute to bloom)
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);

    } else {
        // ════════════════════════════════════════════════════
        //  MODE 1: GAUSSIAN BLUR
        //  Sample the texture at 9 points (center + 4 each side)
        //  Weight each sample by the Gaussian curve.
        //  uHorizontal decides which direction we blur.
        //
        //  texel = size of one pixel in UV space (0..1)
        //  e.g. for 640x360 texture: texel = (1/640, 1/360)
        // ════════════════════════════════════════════════════

        // Size of one texel in UV space
        vec2 texel = 1.0 / vec2(textureSize(uTex, 0));

        // Start with center pixel (weight[0])
        vec3 result = texture(uTex, vUV).rgb * weight[0];

        if (uHorizontal == 1) {
            // ------------ Horizontal blur (left and right) ------------
            // Sample 4 pixels to the right, 4 to the left
            for (int i = 1; i < 5; i++) {
                // Right side sample
                result += texture(uTex,
                    vUV + vec2(texel.x * i, 0.0)).rgb * weight[i];
                // Left side sample (mirror)
                result += texture(uTex,
                    vUV - vec2(texel.x * i, 0.0)).rgb * weight[i];
            }
        } else {
            // --------------- Vertical blur (up and down) --------------
            // Sample 4 pixels above, 4 below
            for (int i = 1; i < 5; i++) {
                // Above sample
                result += texture(uTex,
                    vUV + vec2(0.0, texel.y * i)).rgb * weight[i];
                // Below sample (mirror)
                result += texture(uTex,
                    vUV - vec2(0.0, texel.y * i)).rgb * weight[i];
            }
        }

        fragColor = vec4(result, 1.0);
    }
}