#pragma once

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#include "Constants.hpp"

#if defined(NO_LOCAL_CONFIG)
    #warning "Not Including local config"

// Don't load local configuration file
#elif defined(MATRIX_LOCAL_CONFIG)
    #warning "Including Matrix config"
    #include "Configuration_local_matrix.hpp"
#elif BOARD == BOARD_AVR_MEGA2560 && __has_include("Configuration_local_mega2560.hpp")
    #warning "Including Mega2560 config"
    #include "Configuration_local_mega2560.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V21 && __has_include("Configuration_local_mksgenlv21.hpp")
    #warning "Including MKS GenL V2.1 config"
    #include "Configuration_local_mksgenlv21.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V2 && __has_include("Configuration_local_mksgenlv2.hpp")
    #warning "Including MKS GenL V2.0 config"
    #include "Configuration_local_mksgenlv2.hpp"
#elif BOARD == BOARD_AVR_MKS_GEN_L_V1 && __has_include("Configuration_local_mksgenlv1.hpp")
    #warning "Including MKS GenL V1.0 config"
    #include "Configuration_local_mksgenlv1.hpp"
#elif BOARD == BOARD_ESP32_ESP32DEV && __has_include("Configuration_local_esp32dev.hpp")
    #warning "Including ESP32 config"
    #include "Configuration_local_esp32dev.hpp"
#elif BOARD == BOARD_STM32_F401RE && __has_include("Configuration_local_stm32f401re.hpp")
    #warning "Including STM32 401 config"
    #include "Configuration_local_stm32f401re.hpp"
#elif BOARD == BOARD_STM32_F446RE && __has_include("Configuration_local_stm32f446re.hpp")
    #warning "Including STM32 446 config"
    #include "Configuration_local_stm32f446re.hpp"
#elif __has_include("Configuration_local.hpp")
    #warning "Including non-board specific local config"
    #include "Configuration_local.hpp"
#else
    #pragma message "Not Including any local config for board " STR(BOARD)
#endif
