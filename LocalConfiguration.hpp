#pragma once

#include "Constants.hpp"

#if defined(NO_LOCAL_CONFIG)
    // Don't load local configuration file
#elif defined(MATRIX_LOCAL_CONFIG)
    #include "Configuration_local_matrix.hpp"
#elif BOARD == BOARD_AVR_MEGA2560 && __has_include("Configuration_local_mega2560.hpp")
    #include "Configuration_local_mega2560.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V21 && __has_include("Configuration_local_mksgenlv21.hpp")
    #include "Configuration_local_mksgenlv21.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V2 && __has_include("Configuration_local_mksgenlv2.hpp")
    #include "Configuration_local_mksgenlv2.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V1 && __has_include("Configuration_local_mksgenlv1.hpp")
    #include "Configuration_local_mksgenlv1.hpp"
#elif BOARD == BOARD_ESP32_ESP32DEV && __has_include("Configuration_local_esp32dev.hpp")
    #include "Configuration_local_esp32dev.hpp"
#elif __has_include("Configuration_local.hpp")
    #include "Configuration_local.hpp"
#endif