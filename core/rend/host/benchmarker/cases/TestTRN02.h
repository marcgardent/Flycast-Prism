#pragma once

class TestTRN02 : public TestCase {
public:
    std::string getId() const override { return "TRN-02"; }
    std::string getName() const override { return "Alpha Test / Punch-Through Bug"; }
    std::string getDescription() const override { 
        return "Draws a solid red triangle in the background, and a fully invisible (Alpha=0) green triangle in front of it. "
               "Validates that invisible pixels do not update the Z-buffer or cull geometry behind them. "
               "If the rasterizer does ISP depth-checks before evaluating Alpha, the red triangle will have a hole cut out of it."; 
    }
    std::string getExpected() const override { 
        return "A complete, solid red triangle. The invisible green triangle should have zero visual impact."; 
    }

    void prepare(TestData& data) override {
        // Background: Solid Red Triangle (Z = 0.2, Far)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 200.0f, 100.0f, 0.2f, {255, 0, 0, 255} };
            batch.vertices[1] = { 400.0f, 100.0f, 0.2f, {255, 0, 0, 255} };
            batch.vertices[2] = { 300.0f, 300.0f, 0.2f, {255, 0, 0, 255} };
            batch.indices = { 0, 1, 2 };
            batch.cullMode = FLYCAST_CULL_NONE;
            // Mode: Opaque
            data.batches.push_back(batch);
        }

        // Foreground: Invisible Green Triangle (Z = 0.8, Near, Alpha = 0)
        // With current rasterizer.wgsl, this will win the max_z test during ISP,
        // then render with 0 alpha during TSP, effectively erasing the red triangle behind it!
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 250.0f, 150.0f, 0.8f, {0, 255, 0, 0} }; // Alpha = 0
            batch.vertices[1] = { 450.0f, 150.0f, 0.8f, {0, 255, 0, 0} }; // Alpha = 0
            batch.vertices[2] = { 350.0f, 350.0f, 0.8f, {0, 255, 0, 0} }; // Alpha = 0
            batch.indices = { 0, 1, 2 };
            batch.cullMode = FLYCAST_CULL_NONE;
            // Mode: Punch-Through / Translucent
            data.batches.push_back(batch);
        }
    }
};