#pragma once

#include "unity.h"
#include "Sidereal.hpp"

namespace test {
    namespace sidereal {

        void test_calculate_from_lst()
        {
            DayTime ha_zero = Sidereal::calculateHa(0.0);
            TEST_ASSERT_EQUAL_INT16(2, ha_zero.getHours());
            TEST_ASSERT_EQUAL_INT16(47, ha_zero.getMinutes());
            TEST_ASSERT_EQUAL_INT16(44, ha_zero.getSeconds());
        }

        void run() {
            RUN_TEST(test_calculate_from_lst);
        }
    }
}