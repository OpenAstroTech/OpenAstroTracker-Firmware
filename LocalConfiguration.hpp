#pragma once

#include "Constants.hpp"

#if defined(NO_LOCAL_CONFIG)
// Don't load local configuration file
#elif defined(MATRIX_LOCAL_CONFIG)
    #include "Configuration_local_matrix.hpp"
#elif BOARD == BOARD_AVR_RAMPS && __has_include("Configuration_local_ramps.hpp")
    #include "Configuration_local_ramps.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V21 && __has_include("Configuration_local_mksgenlv21.hpp")
    #include "Configuration_local_mksgenlv21.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V2 && __has_include("Configuration_local_mksgenlv2.hpp")
    #include "Configuration_local_mksgenlv2.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V1 && __has_include("Configuration_local_mksgenlv1.hpp")
    #include "Configuration_local_mksgenlv1.hpp"
#elif BOARD == BOARD_ESP32_ESP32DEV && __has_include("Configuration_local_esp32dev.hpp")
    #include "Configuration_local_esp32dev.hpp"
#elif BOARD == BOARD_RP2040_JACKW01 && __has_include("Configuration_local_rp2040_jackw01.hpp")
    #include "Configuration_local_rp2040_jackw01.hpp"
#elif BOARD == BOARD_RP2350_JACKW01 && __has_include("Configuration_local_rp2350_jackw01.hpp")
    #include "Configuration_local_rp2350_jackw01.hpp"
#elif BOARD == BOARD_RP2040_SKR_PICO && __has_include("Configuration_local_rp2040_skr_pico.hpp")
    #include "Configuration_local_rp2040_skr_pico.hpp"
// #elif BOARD == BOARD_RP2040_PICO && __has_include("Configuration_local_rp2040_pico.hpp")  // NOT YET SUPPORTED
//     #include "Configuration_local_rp2040_pico.hpp"
// #elif BOARD == BOARD_RP2350_PICO2 && __has_include("Configuration_local_rp2350_pico2.hpp")  // NOT YET SUPPORTED
//     #include "Configuration_local_rp2350_pico2.hpp"
#elif __has_include("Configuration_local.hpp")
    #include "Configuration_local.hpp"
#endif