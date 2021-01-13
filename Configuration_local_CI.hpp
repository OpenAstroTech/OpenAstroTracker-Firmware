/**
 * Configuration_local_CI.hpp - Local configuration file to support GitHub CI process.
 * 
 * Assumes that the CI environment will inject the following directives:
 *  RA_STEPPER_TYPE
 *  DEC_STEPPER_TYPE
 *  RA_DRIVER_TYPE
 *  DEC_DRIVER_TYPE
 *  USE_GPS
 *  USE_GYRO_LEVEL
 *  AZIMUTH_ALTITUDE_MOTORS
 *  DISPLAY_TYPE
 * 
 * For the possible ranges of the directives above, this file will generate appropriate pin
 * assignments and resource mappings to allow the CI process to execute. 
 * The assignments in this file may not work on real hardware.
 */

// For testing purposes only
#if 0
    #define RA_STEPPER_TYPE 1
    #define DEC_STEPPER_TYPE 1
    #define RA_DRIVER_TYPE 3
    #define DEC_DRIVER_TYPE 3
    #define USE_GPS 0
    #define USE_GYRO_LEVEL 0
    #define DISPLAY_TYPE 0

    #define AZIMUTH_ALTITUDE_MOTORS 0
    #define AZ_STEPPER_TYPE 1
    #define AZ_DRIVER_TYPE 3
    #define ALT_STEPPER_TYPE 1
    #define ALT_DRIVER_TYPE 3
#endif

/// RA_STEPPER_TYPE
#if (RA_STEPPER_TYPE == STEPPER_TYPE_28BYJ48)
    // Nothing to do
#elif (RA_STEPPER_TYPE == STEPPER_TYPE_NEMA17)
    // Nothing to do
#else
    #error Unrecognized RA stepper type.
#endif

/// DEC_STEPPER_TYPE
#if (DEC_STEPPER_TYPE == STEPPER_TYPE_28BYJ48)
    // Nothing to do
#elif (DEC_STEPPER_TYPE == STEPPER_TYPE_NEMA17)
    // Nothing to do
#else
    #error Unrecognized DEC stepper type.
#endif

/// RA_DRIVER_TYPE
#if (RA_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // RA driver pins for ULN2003 driver
    #define RA_IN1_PIN (-1)  
    #define RA_IN2_PIN (-1)  
    #define RA_IN3_PIN (-1) 
    #define RA_IN4_PIN (-1)  
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // RA driver pins for A4988, TMC2209_STANDALONE drivers
    #define RA_STEP_PIN (-1)  
    #define RA_DIR_PIN  (-1)  
    #define RA_EN_PIN   (-1)  
    #define RA_DIAG_PIN (-1)
    #define RA_MS0_PIN  (-1)  
    #define RA_MS1_PIN  (-1)  
    #define RA_MS2_PIN  (-1)  
#elif (RA_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // RA driver pins for TMC2209_UART drivers
    #define RA_STEP_PIN (-1)  
    #define RA_DIR_PIN  (-1)  
    #define RA_EN_PIN   (-1)  
    #define RA_DIAG_PIN (-1)  
    #if defined(__AVR_ATmega2560__)
        // Additional SoftSerial pins required
        #define RA_SERIAL_PORT_TX (-1)  
        #define RA_SERIAL_PORT_RX  (-1)  
    #endif
    // Additional configuration
    #define RA_DRIVER_ADDRESS (0)
#else
    #error Unrecognized RA driver type.
#endif

/// DEC_DRIVER_TYPE
#if (DEC_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
    // DEC driver pins for ULN2003 driver
    #define DEC_IN1_PIN (-1)  
    #define DEC_IN2_PIN (-1)  
    #define DEC_IN3_PIN (-1) 
    #define DEC_IN4_PIN (-1)  
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
    // DEC driver pins for A4988, TMC2209_STANDALONE drivers
    #define DEC_STEP_PIN (-1)  
    #define DEC_DIR_PIN  (-1)  
    #define DEC_EN_PIN   (-1)  
    #define DEC_DIAG_PIN (-1)     
    #define DEC_MS0_PIN  (-1)  
    #define DEC_MS1_PIN  (-1)  
    #define DEC_MS2_PIN  (-1)  
#elif (DEC_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
    // DEC driver pins for TMC2209_UART drivers
    #define DEC_STEP_PIN (-1)  
    #define DEC_DIR_PIN  (-1)  
    #define DEC_EN_PIN   (-1)  
    #define DEC_DIAG_PIN (-1)  
    #if defined(__AVR_ATmega2560__)
        // Additional SoftSerial pins required
        #define DEC_SERIAL_PORT_TX (-1)  
        #define DEC_SERIAL_PORT_RX  (-1)  
    #endif
    // Additional configuration
    #define DEC_DRIVER_ADDRESS (0)
#else
    #error Unrecognized DEC driver type.
#endif

/// USE_GPS
#if (USE_GPS == 0)
    // Nothing to do - GPS not used
#elif (USE_GPS == 1)
    // GPS uses predefined hardware serial ports used on all platforms - Nothing to do here
#else
    #error Unrecognized GPS configuration.
#endif

/// USE_GYRO_LEVEL
#if (USE_GYRO_LEVEL == 0)
    // Nothing to do - Gyro not used
#elif (USE_GYRO_LEVEL == 1)
    // Gyro uses I2C on all platforms - Nothing to do here
#else
    #error Unrecognized gyro configuration.
#endif

/// AZIMUTH_ALTITUDE_MOTORS
#if (AZIMUTH_ALTITUDE_MOTORS == 1)
    #if (AZ_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
        // AZ driver pins for ULN2003 driver
        #define AZ_IN1_PIN (-1)  
        #define AZ_IN2_PIN (-1)  
        #define AZ_IN3_PIN (-1) 
        #define AZ_IN4_PIN (-1)  
    #elif (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (AZ_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
        // AZ driver pins for A4988, TMC2209_STANDALONE drivers, or TMC2209_UART drivers
        #define AZ_STEP_PIN (-1)  
        #define AZ_DIR_PIN  (-1)  
        #define AZ_EN_PIN   (-1)  
        #define AZ_DIAG_PIN (-1)  
        #if (AZ_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
          // Additional SoftSerial pins required
          #define AZ_DRIVER_ADDRESS (0)
          #define AZ_SERIAL_PORT_TX (-1)  
          #define AZ_SERIAL_PORT_RX  (-1)  
        #endif
    #else
        #error Unrecognized AZ driver type.
    #endif

    #if (ALT_DRIVER_TYPE == DRIVER_TYPE_ULN2003)
        // ALT driver pins for ULN2003 driver
        #define ALT_IN1_PIN (-1)  
        #define ALT_IN2_PIN (-1)  
        #define ALT_IN3_PIN (-1) 
        #define ALT_IN4_PIN (-1)  
    #elif (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART) || (ALT_DRIVER_TYPE == DRIVER_TYPE_A4988_GENERIC) || (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_STANDALONE)
        // ALT driver pins for A4988, TMC2209_STANDALONE drivers, or TMC2209_UART drivers
        #define ALT_STEP_PIN (-1)  
        #define ALT_DIR_PIN  (-1)  
        #define ALT_EN_PIN   (-1)  
        #define ALT_DIAG_PIN (-1)  
        #if (ALT_DRIVER_TYPE == DRIVER_TYPE_TMC2209_UART)
          // Additional SoftSerial pins required
          #define ALT_DRIVER_ADDRESS (0)
          #define ALT_SERIAL_PORT_TX (-1)  
          #define ALT_SERIAL_PORT_RX  (-1)
        #endif
    #else
        #error Unrecognized ALT driver type.
    #endif
#endif

/// DISPLAY_TYPE (display)
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE)
    // Nothing to do - No display present
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
    #define LCD_BRIGHTNESS_PIN  (-1)  
    #define LCD_PIN4  (-1)        
    #define LCD_PIN5  (-1)     
    #define LCD_PIN6  (-1)       
    #define LCD_PIN7  (-1)          
    #define LCD_PIN8  (-1)          
    #define LCD_PIN9  (-1)                   
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008)
    // Display uses I2C on all platforms - Nothing to do here
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
    // Display uses I2C on all platforms - Nothing to do here
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)   
    // Display uses I2C on all platforms - Nothing to do here
#else
    #error Unrecognized display configuration.
#endif

/// DISPLAY_TYPE (keypad)
#if (DISPLAY_TYPE == DISPLAY_TYPE_NONE)
    // Nothing to do - No display present
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD)
    #define LCD_KEY_SENSE_PIN   (-1)  
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008)
    // Keypad uses I2C on all platforms - Nothing to do here
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017)
    // Keypad uses I2C on all platforms - Nothing to do here
#elif (DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306)
    #define LCD_KEY_SENSE_X_PIN     (-1)          
    #define LCD_KEY_SENSE_Y_PIN     (-1)          
    #define LCD_KEY_SENSE_PUSH_PIN  (-1)                   
#else
    #error Unrecognized keypad configuration.
#endif
