#ifndef _LCDMENU_HPP_
#define _LCDMENU_HPP_

#include "core/menu/MenuModel.hpp"
#include "ports/IMountSettings.hpp"

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    #include <LiquidCrystal.h>
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    #include <LiquidTWI2.h>
#elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    #include <U8x8lib.h>  // https://github.com/olikraus/u8g2
#endif

// Class that drives the LCD screen with a menu
// You add a string and an id item and this class handles the display and navigation
class LcdMenu
{
  public:
    // Create a new menu, using the given number of LCD display columns and rows. `storage` is the
    // caller-owned item table (the model never allocates) and the settings store is where the
    // backlight level is persisted; null keeps the level in RAM only.
    LcdMenu(byte cols, byte rows, core::MenuEntry *storage, byte capacity, ports::IMountSettings *settings = nullptr);

    void startup();

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD && defined(LCD_BRIGHTNESS_PIN)
    // Function to test LCD hardware, some units are shipped with defects
    static bool testIfLcdIsBad();
#endif

    // Add a new menu item to the list (order matters)
    void addItem(const char *disp, byte id);

    // Get the currently active item ID
    byte getActive();

    // Set the active menu item
    void setActive(byte id);

    // Pass thru utility function
    void setCursor(byte col, byte row);

    // Set and get the brightness of the backlight
    void setBacklightBrightness(int level, bool persist = true);
    int getBacklightBrightness() const;
    void getBacklightBrightnessRange(int *minPtr, int *maxPtr) const;

    // Pass thru utility function
    void clear();

    // Go to the next menu item from currently active one
    void setNextActive();

    // Update the display of the LCD with the current menu settings
    // This iterates over the menu items, building a menu string by concatenating their display string.
    // It also places the selector arrows around the active one.
    // It then sends the string to the LCD, keeping the selector arrows centered in the same place.
    void updateDisplay();

    // Print a string to the LCD at the current cursor position, substituting the special arrows and padding with spaces to the end
    void printMenu(String line);

    // Print a character at a specific position
    void printAt(int col, int row, char ch);

#if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    uint8_t readButtons();
#endif

  private:
    // Print a single character at the current cursor location and advance cursor by one. Substitutes special chars.
    void printChar(char ch);

  private:
#if DISPLAY_TYPE > 0

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    LiquidCrystal _lcd;  // The LCD screen that we'll display the menu on
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    LiquidTWI2 _lcd;  // The LCD screen that we'll display the menu on
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    U8X8_SSD1306_128X32_UNIVISION_HW_I2C _lcd;
    #endif

    byte const _cols;
    byte const _rows;
    byte const _charHeightRows;  // Height of character in display native rows
    bool _lcdBadHw;

    core::MenuModel _model;  // The item table the caller handed us, plus the selection
    byte _columns;           // The number of columns in the LCD display
    byte _activeRow;         // The row that the LCD cursor is on
    byte _activeCol;         // The column that the LCD cursor is on
    String _lastDisplay[2];  // The last string that was displayed on each row
    byte _brightness;
    ports::IMountSettings *_settings = nullptr;

    #if DISPLAY_TYPE != DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    enum specialChar_t : byte
    {
        _degrees,
        _minutes,
        _leftArrow,
        _rightArrow,
        _upArrow,
        _downArrow,
        _tracking,
        _noTracking,
        SPECIAL_CHAR_MAX,
    };
    static_assert(SPECIAL_CHAR_MAX <= 8, "LCD only supports a maximum of 8 special characters");

    // The special character bitmaps
    static byte RightArrowBitmap[8];
    static byte LeftArrowBitmap[8];
    static byte UpArrowBitmap[8];
    static byte DownArrowBitmap[8];
    static byte DegreesBitmap[8];
    static byte MinutesBitmap[8];
    static byte TrackingBitmap[8];
    static byte NoTrackingBitmap[8];
    #endif

#endif
};

#endif
