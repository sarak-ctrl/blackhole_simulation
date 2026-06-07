#version 410 core
// ============================================================
//  grid.frag — Spacetime Grid Fragment Shader
// ============================================================
// This shader colors each line of the spacetime grid.
// The color changes based on how close we are to the black hole:
//
//   Far away    = cyan/blue  (spacetime at rest, calm)
//   Mid range   = orange     (noticeable curvature)
//   Very close  = deep red   (extreme gravity, redshift)
//
// This mimics real gravitational redshift:
// light from near a black hole appears redder to distant observers
// ============================================================

// Inputs from grid.vert
in vec3  vWorldPos;      // warped world position
in float vDistToCenter;  // distance from black hole
in float vWarpAmount;    // how much this point was warped

// Final pixel color output
out vec4 fragColor;

// Uniforms from C++
uniform float uTime;  // animation timer
uniform float uMass;  // black hole mass

void main() {
    float r  = vDistToCenter;
    float rs = 2.0 * uMass;  // Schwarzschild radius

    // ------------------ Distance-based fade -------------------
    // Grid fades out far from black hole (looks cleaner)
    float farFade  = 1.0 - smoothstep(10.0, 15.0, r);
    // Grid fades in close to black hole (avoid visual clutter)
    float nearFade = smoothstep(rs * 1.1, rs * 2.5, r);
    float alpha    = farFade * nearFade * 0.65;

    // ------------------ Color gradient: cyan → orange → red ---------------
    // t = 0 means far away (cyan), t = 1 means very close (red)
    float t    = clamp(1.0 - (r - rs) / (rs * 6.0), 0.0, 1.0);

    // Define our three key colors
    vec3 cyan  = vec3(0.0, 0.85, 1.0);   // far: cool calm spacetime
    vec3 orng  = vec3(1.0, 0.45, 0.0);   // mid: warming up
    vec3 red   = vec3(0.9, 0.05, 0.05);  // close: extreme redshift

    // Blend between colors based on distance
    vec3 col;
    if (t < 0.5)
        // Far to mid: cyan → orange
        col = mix(cyan, orng, t * 2.0);
    else
        // Mid to close: orange → red
        col = mix(orng, red, (t - 0.5) * 2.0);

    // -------------- Brightness boost in warped region ---------------
    // Grid lines glow brighter where spacetime is most curved
    // This visually highlights the gravity well
    float glow = 1.0 + vWarpAmount * 2.5;
    col *= glow;

    // ------------------ Subtle pulse animation -------------------
    // Grid lines pulse gently like energy flowing inward
    // sin creates oscillation, the r term makes it ripple outward
    float pulse = 1.0 + 0.08 * sin(r * 3.0 - uTime * 2.0);
    col *= pulse;

    // Output final color with transparency
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}