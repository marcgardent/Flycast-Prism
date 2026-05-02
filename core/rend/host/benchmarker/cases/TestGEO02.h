#pragma once

class TestGEO02 : public TestCase {
public:
    std::string getId() const override { return "GEO-02"; }
    std::string getName() const override { return "Culling"; }
    std::string getDescription() const override { return "Three overlapping triangles with different winding and culling."; }

    void prepare(TestData& data) override {
        // 1. Red Triangle: CCW, Cull None (Visible)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[1] = { 420.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[2] = { 220.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_NONE;
            data.batches.push_back(batch);
        }

        // 2. Green Triangle: CCW, Cull Back (Visible if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 150.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[1] = { 420.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[2] = { 220.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }

        // 3. Blue Triangle: CW, Cull Back (Hidden if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 200.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[1] = { 420.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[2] = { 220.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.indices = { 0, 1, 2 }; // CW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }
    }
};