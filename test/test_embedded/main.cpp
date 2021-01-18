#include <unity.h>

#include "test_sidereal.h"

// void setup()
// {

// }

// void loop()
// {
    
// }

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    test::sidereal::run();

    UNITY_END();

    return 0;
}