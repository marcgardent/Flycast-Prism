#pragma once

class TestGEO07 : public TestCase {
public:
    std::string getId() const override { return "GEO-07"; }
    std::string getName() const override { return "Translucency Overdraw Logic"; }
    std::string getDescription() const override { 
        return "Draws two overlapping translucent triangles within the SAME batch. "
               "Validates that the rasterizer blends all passing fragments. "
               "Current WGSL logic only keeps 'closest_tri' and discards the rest."; 
    }
    std::string getExpected() const override { 
        return "A blend of Blue and Red forming a purple intersection. If bugged, the blue triangle will have a 'hole' where the red one is, because the back triangle is discarded."; 
    }

    void prepare(TestData& data) override {
        // Single Batch containing overlapping geometry
        {
            DrawBatch batch;
            batch.vertices.resize(6);
            
            // Blue triangle (Behind, Z = 0.7)
            batch.vertices[0] = { 200.0f, 100.0f, 0.7f, {0, 0, 255, 128} };
            batch.vertices[1] = { 400.0f, 100.0f, 0.7f, {0, 0, 255, 128} };
            batch.vertices[2] = { 300.0f, 300.0f, 0.7f, {0, 0, 255, 128} };
            
            // Red triangle (Front, Z = 0.4)
            batch.vertices[3] = { 300.0f, 200.0f, 0.4f, {255, 0, 0, 128} };
            batch.vertices[4] = { 500.0f, 200.0f, 0.4f, {255, 0, 0, 128} };
            batch.vertices[5] = { 400.0f, 400.0f, 0.4f, {255, 0, 0, 128} };
            
            batch.indices = { 0, 1, 2, 0xFFFFFFFF, 3, 4, 5 };
            batch.cullMode = FLYCAST_CULL_NONE;
            
            data.batches.push_back(batch);
        }
    }
};