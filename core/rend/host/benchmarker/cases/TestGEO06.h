#pragma once

class TestGEO06 : public TestCase {
public:
    std::string getId() const override { return "GEO-06"; }
    std::string getName() const override { return "Perspective-Correct Interpolation"; }
    std::string getDescription() const override { 
        return "Draws a large quad (2 triangles) tilted deeply into the Z-axis. "
               "Tests if the shader uses perspective-correct barycentric interpolation (1/W). "
               "If using affine screen-space interpolation (like PS1), the diagonal line separating the triangles will be extremely obvious and colors/textures will warp."; 
    }
    std::string getExpected() const override { 
        return "A smooth gradient across the entire quad. NO visible diagonal line cutting the quad in half."; 
    }

    void prepare(TestData& data) override {
        // A Floor plane extending into the distance
        // Z=0.9 is near the camera, Z=0.1 is far away.
        {
            DrawBatch batch;
            batch.vertices.resize(4);
            
            // Bottom edge (Near camera) -> Z = 0.9
            batch.vertices[0] = { 100.0f, 400.0f, 0.9f, {255, 0, 0, 255} }; // Red
            batch.vertices[1] = { 540.0f, 400.0f, 0.9f, {0, 255, 0, 255} }; // Green
            
            // Top edge (Far away) -> Z = 0.1
            // Notice the X coordinates converge to simulate a perspective vanishing point
            batch.vertices[2] = { 250.0f, 100.0f, 0.1f, {0, 0, 255, 255} }; // Blue
            batch.vertices[3] = { 390.0f, 100.0f, 0.1f, {255, 255, 0, 255} }; // Yellow
            
            // Draw as a Quad (2 Triangles)
            batch.indices = { 0, 1, 2, 2, 1, 3 }; 
            batch.cullMode = FLYCAST_CULL_NONE;
            
            data.batches.push_back(batch);
        }
    }
};