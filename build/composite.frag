#version 410 core
// ============================================================
//  composite.frag — Final Composite Shader
// ============================================================
// This is the LAST step in the render pipeline.
// It takes two textures and combines them into the final image:
//
//   uScene = the raw HDR black hole render
//   uBloom = the blurred glow (bloom) texture
//
// Three things happen here:
//   1. BLOOM ADD    — add the glow on top of the scene
//   2. TONE MAPPING — compress HDR (0..∞) to LDR (0..1)
//                     using the ACES filmic curve
//                     (same algorithm used in movies and AAA games)
//   3. GAMMA        — correct for monitor display (^1/2.2)
//   4. VIGNETTE     — darken screen edges for cinematic feel
// ============================================================

// UV coordinate from bh.vert
in vec2 vUV;

// Final pixel color output to screen
out vec4 fragColor;

// -------------------- Uniforms ---------------------------
uniform sampler2D uScene;     // raw HDR scene texture
uniform sampler2D uBloom;     // blurred bloom texture
uniform float     uBloomStr;  // bloom strength multiplier

// ============================================================
//  ACES FILMIC TONE MAPPING
//  ACES = Academy Color Encoding System
//  Used by Hollywood and game studios (Unreal Engine, etc.)
//  Maps HDR values (can be > 1.0) to visible LDR range (0..1)
//  The curve:
//    - Dark areas: lifted slightly (more detail in shadows)
//    - Mid tones:  roughly linear (accurate colors)
//    - Bright areas: roll off smoothly (no harsh clipping)
//  Without tone mapping, HDR values > 1.0 just clip to white
//  With ACES, they compress beautifully into a visible range
// ============================================================
vec3 aces(vec3 x) {
    // ACES fitted curve constants (Hill 2016)
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

// ============================================================
//  MAIN
// ============================================================
void main() {
    // ----------------- Step 1: Sample both textures -----------------
    vec3 scene = texture(uScene, vUV).rgb;  // raw HDR render
    vec3 bloom = texture(uBloom, vUV).rgb;  // blurred glow

    // ----------------- Step 2: Add bloom on top of scene --------------
    // uBloomStr controls how strong the glow effect is
    // Higher value = more dramatic glow around the disk
    vec3 col = scene + bloom * uBloomStr;

    // ----------------- Step 3: ACES filmic tone mapping -----------------
    // Compresses the HDR colors into displayable 0..1 range
    // This is what makes the image look cinematic rather than
    // blown out or washed out
    col = aces(col);

    // ----------------- Step 4: Gamma correction -------------------
    // Monitors display colors non-linearly (gamma ~2.2)
    // We need to apply the inverse (^1/2.2) so colors look right
    // Without this: image looks too dark and colors are wrong
    col = pow(col, vec3(1.0 / 2.2));

    // ------------------ Step 5: Vignette -------------------
    // Darkens the corners and edges of the screen
    // Draws the eye toward the center (the black hole)
    // Very common cinematic technique
    vec2  uv2 = vUV * 2.0 - 1.0;  // remap to (-1, 1)
    // dot(uv2, uv2) = distance squared from center
    float vig = 1.0 - dot(uv2, uv2) * 0.25;
    // smoothstep makes the vignette edge soft not sharp
    col *= smoothstep(0.0, 0.5, vig);

    // ------------------- Final output ---------------------
    fragColor = vec4(col, 1.0);
}