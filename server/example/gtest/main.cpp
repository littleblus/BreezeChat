#include <gtest/gtest.h>

TEST(test_case_name, test_name) {
    EXPECT_EQ(1, 1);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}