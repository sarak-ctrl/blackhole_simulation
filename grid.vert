#version 410 core
// ============================================================
//  grid.vert — Spacetime Grid Vertex Shader
// ============================================================
// This shader runs once for every VERTEX (corner point) of
// the spacetime grid. It warps each vertex downward to show
// the curvature of spacetime around the black hole.
//
// Three effects applied to each grid vertex:
//   1. Gravitational well — grid dips down near black hole
//      (Flamm's paraboloid — the famous funnel shape)
//   2. Frame dragging — grid twists around spin axis (Kerr)
//   3. Gravitational waves — subtle ripple animation
// ============================================================

// Input: flat 3D position on the XZ plane from buildGrid()
layout(location=0) in vec3 aPos;

// Outputs sent to grid.frag for coloring
out vec3  vWorldPos;      // final warped position in world space
out float vDistToCenter;  // distance from black hole (for coloring)
out float vWarpAmount;    // how much this vertex was warped (for glow)

// Uniforms from C++
uniform mat4  uVP;    // ViewProjection matrix (camera transform)
uniform float uTime;  // animation timer
uniform float uMass;  // black hole mass
uniform float uSpin;  // black hole spin

void main() {
    vec3 p = aPos;  // start with flat grid position

    // Distance from black hole (on XZ plane, ignoring Y)
    float r  = length(p.xz);
    float rs = 2.0 * uMass;  // Schwarzschild radius

    // -------- Effect 1: Gravitational well (Flamm's paraboloid) ------
    // Real spacetime geometry: embedding diagram shows a funnel
    // y dips down as: depth ∝ -M / r
    // We clamp r so vertices don't go to infinity at center
    float rClamped = max(r, rs * 1.05);
    float well     = -uMass * 2.5 / rClamped;

    // Smoothly fade the well effect:
    // - No dip very close to black hole (would be too extreme)
    // - Full dip from rs*3 outward
    well *= smoothstep(rs * 0.8, rs * 3.0, r);
    p.y  += well;

    // ------------- Effect 2: Frame dragging (Kerr rotation) -------------
    // In Kerr spacetime, the black hole drags spacetime around
    // with it like a rotating ball in honey.
    // Effect is strongest close in (1/r^2) and fades far out.
    float twist = uSpin * uMass * 0.6 / max(r * r, 0.5);
    // Add slow time rotation so the grid visually spins
    float angle = twist + uTime * 0.04 * uSpin;
    float ca = cos(angle);
    float sa = sin(angle);
    // Rotate the XZ position by angle
    float nx = p.x * ca - p.z * sa;
    float nz = p.x * sa + p.z * ca;
    p.x = nx;
    p.z = nz;

    // --------------- Effect 3: Gravitational wave ripples ----------------
    // Aesthetic effect: ripples travel outward from black hole
    // Like dropping a stone in water — but in spacetime!
    float wave = 0.04 * sin(r * 1.2 - uTime * 2.5) *
                 exp(-r * 0.08);  // exp fade: ripples die at distance
    p.y += wave;

    // ---------------------- Pass data to fragment shader ----------------------
    vWorldPos     = p;
    vDistToCenter = r;
    // Normalize warp amount to 0..1 range for color mapping
    vWarpAmount   = abs(well) / (uMass * 2.5 + 0.001);

    // Final screen position = ViewProjection * world position
    gl_Position = uVP * vec4(p, 1.0);
}