#include <unity.h>

#include "test_sidereal.h"

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    test::sidereal::run();

    UNITY_END();

    return 0;
}