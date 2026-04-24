#include <gtest/gtest.h>
#include "rend/vulkan/gbuffer/PolyRequestEvaluator.h"

using namespace rend;

TEST(PolyRequestEvaluatorTest, BasicEvaluation) {
    PolyRequestEvaluator evaluator;
    PolyData data = {};
    data.wp_x = 10.0;
    data.wp_y = 20.0;
    data.wp_z = 30.0;
    data.texture_hash = 0x12345678;
    data.poly_count = 100;
    data.material_id = 0x40; // MID 0x40 -> (0x40 >> 4) & 0x7 = 4 (PUNCH_THROUGH)

    EXPECT_TRUE(evaluator.parse("WP_X == 10 and WP_Y == 20 and WP_Z == 30"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    EXPECT_TRUE(evaluator.parse("WP_Z < 100"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    EXPECT_TRUE(evaluator.parse("TH == 305419896")); // 0x12345678 in decimal
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    EXPECT_TRUE(evaluator.parse("PC >= 100"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    EXPECT_TRUE(evaluator.parse("MID == 64")); // 0x40
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);
}

TEST(PolyRequestEvaluatorTest, MIDFlags) {
    PolyRequestEvaluator evaluator;
    
    // MID = 0x40 (0100 0000) -> ListType=4 (MID_PUNCH_THROUGH), flags=0
    PolyData data1 = {};
    data1.material_id = 0x40;
    EXPECT_TRUE(evaluator.parse("MID_PUNCH_THROUGH"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data1), 1.0);
    EXPECT_TRUE(evaluator.parse("MID_OPAQUE or MID_TRANSLUCENT"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data1), 0.0);

    // MID = 0x2B (0010 1011) -> ListType=2 (MID_TRANSLUCENT), HasTex=1, Gouraud=0, HasBump=1, Fog=1
    PolyData data2 = {};
    data2.material_id = 0x2B;
    EXPECT_TRUE(evaluator.parse("MID_TRANSLUCENT and MID_HAS_TEX and not(MID_GOURAUD) and MID_HAS_BUMP and MID_FOG"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data2), 1.0);
}

TEST(PolyRequestEvaluatorTest, IsClose) {
    PolyRequestEvaluator evaluator;
    PolyData data = {};
    data.wp_x = 1.000001f;

    // Default tolerances (rel_tol=1e-9, abs_tol=0.0 via transpiler)
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 0.0); // 1.000001 is too far for 1e-9

    // Positional rel_tol
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0, 1e-5)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    // Named rel_tol
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0, rel_tol=1e-5)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    // Named abs_tol
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0, abs_tol=1e-5)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);

    // Mixed named arguments
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0, rel_tol=1e-9, abs_tol=1e-5)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);
    
    // Mixed positional and named
    EXPECT_TRUE(evaluator.parse("isclose(WP_X, 1.0, 1e-9, abs_tol=1e-5)"));
    EXPECT_DOUBLE_EQ(evaluator.evaluate(data), 1.0);
}

TEST(PolyRequestEvaluatorTest, InvalidExpression) {
    PolyRequestEvaluator evaluator;
    EXPECT_FALSE(evaluator.parse("WP_X == "));
    EXPECT_FALSE(evaluator.parse("UNKNOWN_VAR"));
}
