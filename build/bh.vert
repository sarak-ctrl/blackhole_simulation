#version 410 core
// ============================================================
//  bh.vert — Vertex Shader (shared by all fullscreen passes)
// ============================================================
// This is the simplest possible vertex shader.
// It takes a 2D position (-1 to 1) and passes it to the screen.
// All the real work happens in the fragment shader (bh.frag).
//
// A "vertex shader" runs once per corner of each triangle.
// We have 6 vertices (2 triangles = 1 fullscreen quad).
// UV coordinates (0 to 1) are passed to the fragment shader
// so it knows which pixel on screen it's working on.
// ============================================================

// Input: 2D position from the quad geometry (-1 to 1 range)
// layout(location=0) means it matches attribute 0 in the C++ code
layout(location=0) in vec2 aPos;

// Output: UV texture coordinate sent to the fragment shader
// vUV ranges from (0,0) bottom-left to (1,1) top-right
out vec2 vUV;

void main() {
    // Convert position from (-1,1) range to (0,1) UV range
    // Example: aPos=(-1,-1) → vUV=(0,0)  aPos=(1,1) → vUV=(1,1)
    vUV = aPos * 0.5 + 0.5;

    // gl_Position is the final screen position of this vertex
    // z=0 means it sits flat on screen, w=1 is standard
    gl_Position = vec4(aPos, 0.0, 1.0);
}