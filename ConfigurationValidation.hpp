#pragma once


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// VALIDATE CONFIGURATION     ////////
//                            ////////
//////////////////////////////////////

// Platform
#if defined(ESP32) || defined(__AVR_ATmega2560__)
  // Valid platform
#else
  #error Unsupported platform configuration. Use at own risk.
#endif

// Display & keypad configurations
#if defined(ESP32) && ((DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306))
  // Valid display for ESP32
#elif defined(__AVR_ATmega2560__) && ((DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD) \
  || (DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017))
  // Valid display for ATmega
#else
  #error Unsupported display configuration. Use at own risk.
#endif

// Validate motor & driver configurations
#if (RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  // Valid RA stepper and driver combination
#elif (RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && ((RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) \
  || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))
  // Valid RA stepper and driver combination
#else
  #error Unsupported RA stepper & driver combination. Use at own risk.
#endif

#if (DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  // Valid DEC stepper and driver combination
#elif (DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && ((DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) \
  || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART))
  // Valid DEC stepper and driver combination
#else
  #error Unsupported DEC stepper & driver combination. Use at own risk.
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #ifndef RA_DRIVER_ADDRESS
    // Serial bus address must be specified for TMC2209 in UART mode
    #error RA driver address for DRIVER_TYPE_TMC2209_UART not specified.
  #endif
#endif

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #ifndef DEC_DRIVER_ADDRESS
    // Serial bus address must be specified for TMC2209 in UART mode
    #error DEC driver address for DRIVER_TYPE_TMC2209_UART not specified.
  #endif
#endif

#if (AZIMUTH_ALTITUDE_MOTORS == 0)
  // Baseline configuration without azimuth & altitude control is valid
#elif defined(__AVR_ATmega2560__)
  // Azimuth configuration
  #if (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid AZ stepper and driver combination
  #elif (AZ_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid AZ stepper and driver combination
  #else
    #error Unsupported AZ stepper & driver combination. Use at own risk.
  #endif

  // Altitude configuration
  #if (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_28BYJ48) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // Valid ALT stepper and driver combination
  #elif (ALT_STEPPER_TYPE == STEPPER_TYPE_NEMA17) && (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // Valid ALT stepper and driver combination
  #else
    #error Unsupported ALT stepper & driver combination. Use at own risk.
  #endif

  #if (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef AZ_DRIVER_ADDRESS
      // Serial bus address must be specified for TMC2209 in UART mode
      #error AZ driver address for DRIVER_TYPE_TMC2209_UART not specified.
    #endif
  #endif

  #if (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #ifndef ALT_DRIVER_ADDRESS
      // Serial bus address must be specified for TMC2209 in UART mode
      #error ALT driver address for DRIVER_TYPE_TMC2209_UART not specified.
    #endif
  #endif

#else
  #error Configuration does not support AZ/ALT. Use at own risk.
#endif 

// Interfaces
#if (BLUETOOTH_ENABLED == 0)
  // Baseline configuration without Bluetooth is valid
#elif defined(ESP32)
  // Bluetooth is only supported on ESP32
  #if !defined(BLUETOOTH_DEVICE_NAME)
    #error Bluetooth device name must be provided
  #endif
#else
  #error Unsupported Bluetooth configuration. Use at own risk.
#endif

#if (WIFI_ENABLED == 0)
  // Baseline configuration without WiFi is valid
#elif defined(ESP32)
  // Wifi is only supported on ESP32
  #if !defined(WIFI_HOSTNAME)
    #error Wifi hostname must be provided for infrastructure and AP modes
  #endif
  #if (WIFI_MODE == WIFI_MODE_DISABLED)
    // Baseline configuration with disabled WiFi is valid
  #endif
  #if (WIFI_MODE == WIFI_MODE_INFRASTRUCTURE) || (WIFI_MODE == WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP)
    #if !defined(WIFI_INFRASTRUCTURE_MODE_SSID) || !defined(WIFI_INFRASTRUCTURE_MODE_WPAKEY)
      #error Wifi SSID and WPA key must be provided for infrastructure mode
    #endif
  #elif (WIFI_MODE == WIFI_MODE_AP_ONLY) || (WIFI_MODE == WIFI_MODE_ATTEMPT_INFRASTRUCTURE_FAIL_TO_AP)
    #if !defined(WIFI_AP_MODE_WPAKEY)
      #error Wifi WPA key must be provided for AP mode
    #endif
  #else
    #error Unsupported WiFi configuration. Use at own risk.
  #endif
#else
  #error Unsupported WiFi configuration (WiFI only supported on ESP32). Use at own risk.
#endif

// External sensors
#if (USE_GPS == 0)
  // Baseline configuration without GPS is valid
#elif defined(ESP32) || defined(__AVR_ATmega2560__)
  // GPS is supported on ESP32 and ATmega
#else
  #error Unsupported GPS configuration. Use at own risk.
#endif

#if (USE_GYRO_LEVEL == 0)
  // Baseline configuration without gyro is valid
#elif defined(ESP32) || defined(__AVR_ATmega2560__)
  // Gyro is supported on ESP32 and ATmega
#else
  #error Unsupported gyro configuration. Use at own risk.
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                            ////////
// VALIDATE PIN ASSIGNMENTS   ////////
//                            ////////
//////////////////////////////////////

// Motor & driver configurations
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #if !defined(DEC_IN1_PIN) || !defined(DEC_IN2_PIN) || !defined(DEC_IN3_PIN) || !defined(DEC_IN4_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_ULN2003 driver
  #endif
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #if !defined(DEC_STEP_PIN) || !defined(DEC_DIR_PIN) || !defined(DEC_EN_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
  #endif
  #if (!defined(DEC_MS0_PIN) || !defined(DEC_MS1_PIN) || !defined(DEC_MS2_PIN)) && (BOARD != BOARD_AVR_MKS_GEN_L_V1)
     #warning Missing pin assignments for MS pins
  #endif
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if !defined(DEC_STEP_PIN) || !defined(DEC_DIR_PIN) || !defined(DEC_EN_PIN) || !defined(DEC_DIAG_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured DEC DRIVER_TYPE_TMC2209_UART driver
  #endif
  #if !((defined(DEC_SERIAL_PORT_TX) && defined(DEC_SERIAL_PORT_RX)) || defined(DEC_SERIAL_PORT))
     // Required pin assignments missing for UART serial
     #error Missing pin assignments for configured DEC DRIVER_TYPE_TMC2209_UART driver serial connection
  #endif
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
  #if !defined(RA_IN1_PIN) || !defined(RA_IN2_PIN) || !defined(RA_IN3_PIN) || !defined(RA_IN4_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured RA DRIVER_TYPE_ULN2003 driver
  #endif
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
  #if !defined(RA_STEP_PIN) || !defined(RA_DIR_PIN) || !defined(RA_EN_PIN)
     // Required pin assignments missing
     #error Missing pin assignments for configured RA DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
  #endif
  #if (!defined(RA_MS0_PIN) || !defined(RA_MS1_PIN) || !defined(RA_MS2_PIN)) && (BOARD != BOARD_AVR_MKS_GEN_L_V1)
     #warning Missing pin assignments for MS pins
  #endif
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if !defined(RA_STEP_PIN) || !defined(RA_DIR_PIN) || !defined(RA_EN_PIN) || !defined(RA_DIAG_PIN)
     // Required pin assignments missing 
     #error Missing pin assignments for configured RA DRIVER_TYPE_TMC2209_UART driver
  #endif
  #if !((defined(RA_SERIAL_PORT_TX) && defined(RA_SERIAL_PORT_RX)) || defined(RA_SERIAL_PORT))
     // Required pin assignments missing for UART serial
     #error Missing pin assignments for configured RA DRIVER_TYPE_TMC2209_UART driver serial connection
  #endif
#endif

#if (AZIMUTH_ALTITUDE_MOTORS == 1)
  #if (AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    #if !defined(AZ_IN1_PIN) || !defined(AZ_IN2_PIN) || !defined(AZ_IN3_PIN) || !defined(AZ_IN4_PIN)
      // Required pin assignments missing
      #error Missing pin assignments for configured AZ DRIVER_TYPE_ULN2003 driver
    #endif
  #elif (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #if !defined(AZ_STEP_PIN) || !defined(AZ_DIR_PIN) || !defined(AZ_EN_PIN) || !defined(AZ_DIAG_PIN)
       // Required pin assignments missing
       #error Missing pin assignments for configured AZ DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
    #endif
  #elif (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if !defined(AZ_STEP_PIN) || !defined(AZ_DIR_PIN) || !defined(AZ_EN_PIN) || !defined(AZ_DIAG_PIN) || !defined(AZ_SERIAL_PORT_TX) || !defined(AZ_SERIAL_PORT_RX)
      // Required pin assignments missing (ATmega uses SoftwareSerial for this driver)
      #error Missing pin assignments for configured AZ DRIVER_TYPE_TMC2209_UART driver
    #endif
    #if !((defined(AZ_SERIAL_PORT_TX) && defined(AZ_SERIAL_PORT_RX)) || defined(AZ_SERIAL_PORT))
       // Required pin assignments missing for UART serial
       #error Missing pin assignments for configured AZ DRIVER_TYPE_TMC2209_UART driver serial connection
    #endif
  #endif

  #if (ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    #if !defined(ALT_IN1_PIN) || !defined(ALT_IN2_PIN) || !defined(ALT_IN3_PIN) || !defined(ALT_IN4_PIN)
      // Required pin assignments missing
      #error Missing pin assignments for configured ALT DRIVER_TYPE_ULN2003 driver
    #endif
  #elif (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    #if !defined(ALT_STEP_PIN) || !defined(ALT_DIR_PIN) || !defined(ALT_EN_PIN) || !defined(ALT_DIAG_PIN)
       // Required pin assignments missing
       #error Missing pin assignments for configured AZ DRIVER_TYPE_A4988_GENERIC or DRIVER_TYPE_TMC2209_STANDALONE driver
    #endif
  #elif (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if !defined(ALT_STEP_PIN) || !defined(ALT_DIR_PIN) || !defined(ALT_EN_PIN) || !defined(ALT_DIAG_PIN) || !defined(ALT_SERIAL_PORT_TX) || !defined(ALT_SERIAL_PORT_RX)
      // Required pin assignments missing (ATmega uses SoftwareSerial for this driver)
      #error Missing pin assignments for configured ALT DRIVER_TYPE_TMC2209_UART driver
    #endif
    #if !((defined(ALT_SERIAL_PORT_TX) && defined(ALT_SERIAL_PORT_RX)) || defined(ALT_SERIAL_PORT))
       // Required pin assignments missing for UART serial
       #error Missing pin assignments for configured ALT DRIVER_TYPE_TMC2209_UART driver serial connection
    #endif
  #endif
#endif

// Displays
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
  // No dedicated pins required apart from I2C
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
  #if !defined(LCD_PIN4) || !defined(LCD_PIN5) || !defined(LCD_PIN6) || !defined(LCD_PIN7)  || !defined(LCD_PIN8) || !defined(LCD_PIN9)
     // Required pin assignments missing
     #error Missing pin assignments for configured DISPLAY_TYPE_LCD_KEYPAD display
  #endif
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)
  // No dedicated pins required apart from I2C for display
#endif

// Keypad
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008) || (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
  // No dedicated pins required apart from I2C
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
  #if !defined(LCD_KEY_SENSE_PIN)
     // Required pin assignments missing
     #error Missing sense pin assignment for configured DISPLAY_TYPE_LCD_KEYPAD keypad
  #endif
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)
  #if !defined(LCD_KEY_SENSE_X_PIN) || !defined(LCD_KEY_SENSE_Y_PIN) || !defined(LCD_KEY_SENSE_PUSH_PIN)
     // Required pin assignments missing
     #error Missing sense pin assignments for configured DISPLAY_TYPE_LCD_JOY_I2C_SSD1306 joystick
  #endif
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                ////////
// VALIDATE CRITICAL PARAMETERS   ////////
//                                ////////
//////////////////////////////////////////

#if (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if defined(DEC_MOTOR_CURRENT_RATING)
    #if (DEC_MOTOR_CURRENT_RATING > 1700)
      #error "The TMC2209 driver is only rated up to 1.7A output. Delete this error if you know what youre doing" 
    #endif
    #if (DEC_MOTOR_CURRENT_RATING == 0)
      #error "DEC current rating/setting is not configured. Enter the current rating of your motor" 
    #endif
  #else
      #error "DEC_MOTOR_CURRENT_RATING not defined."
  #endif
  #if  defined(DEC_OPERATING_CURRENT_SETTING)
    #if (DEC_OPERATING_CURRENT_SETTING <= 0) || (DEC_OPERATING_CURRENT_SETTING > 100)
      #error "DEC_OPERATING_CURRENT_SETTING is not within acceptable range"
    #endif
  #else
      #error "DEC_OPERATING_CURRENT_SETTING not defined."
  #endif
#endif

#if (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
  #if defined(RA_MOTOR_CURRENT_RATING)
    #if (RA_MOTOR_CURRENT_RATING > 1700)
      #error "The TMC2209 driver is only rated up to 1.7A output. Delete this error if you know what youre doing" 
    #endif
    #if (RA_MOTOR_CURRENT_RATING == 0)
      #error "RA current rating/setting is not configured. Enter the current rating of your motor" 
    #endif
  #else
      #error "RA_MOTOR_CURRENT_RATING not defined."
  #endif
  #if  defined(RA_OPERATING_CURRENT_SETTING)
    #if (RA_OPERATING_CURRENT_SETTING <= 0) || (RA_OPERATING_CURRENT_SETTING > 100)
      #error "RA_OPERATING_CURRENT_SETTING is not within acceptable range"
    #endif
  #else
      #error "RA_OPERATING_CURRENT_SETTING not defined."
  #endif
#endif

#if (AZIMUTH_ALTITUDE_MOTORS == 1)
  #if (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(AZ_MOTOR_CURRENT_RATING)
      #if (AZ_MOTOR_CURRENT_RATING > 1700)
        #error "The TMC2209 driver is only rated up to 1.7A output. Delete this error if you know what youre doing" 
      #endif
      #if (AZ_MOTOR_CURRENT_RATING == 0)
        #error "AZ current rating/setting is not configured. Enter the current rating of your motor" 
      #endif
    #else
        #error "AZ_MOTOR_CURRENT_RATING not defined."
    #endif
    #if  defined(AZ_OPERATING_CURRENT_SETTING)
      #if (AZ_OPERATING_CURRENT_SETTING <= 0) || (AZ_OPERATING_CURRENT_SETTING > 100)
        #error "AZ_OPERATING_CURRENT_SETTING is not within acceptable range"
      #endif
    #else
        #error "AZ_OPERATING_CURRENT_SETTING not defined."
    #endif
  #endif
  #if (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    #if defined(ALT_MOTOR_CURRENT_RATING)
      #if (ALT_MOTOR_CURRENT_RATING > 1700)
        #error "The TMC2209 driver is only rated up to 1.7A output. Delete this error if you know what youre doing" 
      #endif
      #if (ALT_MOTOR_CURRENT_RATING == 0)
        #error "ALT current rating/setting is not configured. Enter the current rating of your motor" 
      #endif
    #else
        #error "ALT_MOTOR_CURRENT_RATING not defined."
    #endif
    #if  defined(ALT_OPERATING_CURRENT_SETTING)
      #if (ALT_OPERATING_CURRENT_SETTING <= 0) || (ALT_OPERATING_CURRENT_SETTING > 100)
        #error "ALT_OPERATING_CURRENT_SETTING is not within acceptable range"
      #endif
    #else
        #error "ALT_OPERATING_CURRENT_SETTING not defined."
    #endif
  #endif
#endif