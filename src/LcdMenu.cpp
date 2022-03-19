#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "Utility.hpp"
#include "libs/MappedDict/MappedDict.hpp"
#include "EPROMStore.hpp"
#include "LcdMenu.hpp"

#if DISPLAY_TYPE != DISPLAY_TYPE_NONE

    // Class that drives the LCD screen with a menu
    // You add a string and an id item and this class handles the display and navigation
    // Create a new menu, using the given number of LCD display columns and rows
    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
LcdMenu::LcdMenu(byte cols, byte rows, int maxItems)
    : _lcd(LCD_PIN8, LCD_PIN9, LCD_PIN4, LCD_PIN5, LCD_PIN6, LCD_PIN7), _cols(cols), _rows(rows), _maxItems(maxItems),
      _charHeightRows(1)  // 1 character = 1 row
{
}
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
LcdMenu::LcdMenu(byte cols, byte rows, int maxItems)
    : _lcd(0x20), _cols(cols), _rows(rows), _maxItems(maxItems), _charHeightRows(1)  // 1 character = 1 row
{
        #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    _lcd.setMCPType(LTI_TYPE_MCP23017);
        #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008
    _lcd.setMCPType(LTI_TYPE_MCP23008);
        #endif
}
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
LcdMenu::LcdMenu(byte cols, byte rows, int maxItems)
    : _cols(cols), _rows(rows), _maxItems(maxItems), _charHeightRows(2)  // For 7x14 font 1 character = 2 rows (2x8 pixels)
{
}
    #endif

void LcdMenu::startup()
{
    LOG(DEBUG_INFO, "[LCD]: LcdMenu startup");

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    _lcd.begin(_cols, _rows);
        #if defined(LCD_BRIGHTNESS_PIN)
    _lcdBadHw = testIfLcdIsBad();
        #endif
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    _lcd.begin(_cols, _rows);
    _lcd.setBacklight(RED);
    _lcdBadHw = false;
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    _lcd.begin();
    _lcd.setPowerSave(0);
    _lcd.clear();
    _lcd.setFont(u8x8_font_7x14_1x2_f);  // Each 7x14 character takes up 2 8-pixel rows
    _lcdBadHw = false;
    #endif

    _brightness = EEPROMStore::getBrightness();
    LOG(DEBUG_INFO, "[LCD]: Brightness from EEPROM is %d", _brightness);
    setBacklightBrightness(_brightness, false);

    _numMenuItems    = 0;
    _activeMenuIndex = 0;
    _longestDisplay  = 0;
    _columns         = _cols;
    _activeRow       = -1;
    _activeCol       = -1;
    _lastDisplay[0]  = "";
    _lastDisplay[1]  = "";
    _menuItems       = new MenuItem *[_maxItems];

    #if DISPLAY_TYPE != DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    // Create special characters for degrees and arrows
    _lcd.createChar(_degrees, DegreesBitmap);
    _lcd.createChar(_minutes, MinutesBitmap);
    _lcd.createChar(_leftArrow, LeftArrowBitmap);
    _lcd.createChar(_rightArrow, RightArrowBitmap);
    _lcd.createChar(_upArrow, UpArrowBitmap);
    _lcd.createChar(_downArrow, DownArrowBitmap);
    _lcd.createChar(_tracking, TrackingBitmap);
    _lcd.createChar(_noTracking, NoTrackingBitmap);
    #endif
}

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD && defined(LCD_BRIGHTNESS_PIN)
/**
 * @brief Check to see if there is a problem in the LCD backlight circuit
 * @details The 'broken' designs connect D10 directly to the base of
 * an NPN transistor. This will cause a short when D10 is set to HIGH as there
 * is no current limiting resistor in the path between D10 to the base and the
 * emitter to ground.
 * Adapted from https://forum.arduino.cc/index.php?topic=96747.0
 * Link also notes a HW fix that can be applied to enable full brightness.
 * @returns true if the LCD was manufactured incorrectly, false otherwise
 */
bool LcdMenu::testIfLcdIsBad()
{
    /*
     * Set the pin to an input with pullup disabled, this should be safe on all shields.
     * The reason for the digitalWrite() first is that only the newer Arduino
     * cores disable the pullup when setting the pin to INPUT.
     * On boards that have a pullup on the transistor base,
     * this should cause the backlight to be on.
     */
    digitalWrite(LCD_BRIGHTNESS_PIN, LOW);
    pinMode(LCD_BRIGHTNESS_PIN, INPUT);

    /*
     * Since the pullup was turned off above by setting the pin to input mode,
     * it should drive the pin LOW which should be safe given the known design flaw.
     */
    pinMode(LCD_BRIGHTNESS_PIN, OUTPUT);

    /*
     * !!! WARNING !!!
     * This line is NOT safe thing to use on the broken designs!
     */
    digitalWrite(LCD_BRIGHTNESS_PIN, HIGH);

    // Now see if a short is pulling down the HIGH output.
    delayMicroseconds(5);  // Give some time for the signal to drop
    const int pinValue = digitalRead(LCD_BRIGHTNESS_PIN);

    // Restore the pin to a safe state: Input with pullup turned off
    digitalWrite(LCD_BRIGHTNESS_PIN, LOW);
    pinMode(LCD_BRIGHTNESS_PIN, INPUT);

    /*
     * If the level read back is not HIGH then there is a problem because the
     * pin is being driven HIGH by the AVR.
     */
    const bool lcdIsBad = (pinValue != HIGH);
    LOG(DEBUG_INFO, "[LCD]: HW is bad? %s", lcdIsBad ? "YES" : "NO");
    return lcdIsBad;
}
    #endif

// Find a menu item by its ID
MenuItem *LcdMenu::findById(byte id)
{
    for (byte i = 0; i < _numMenuItems; i++)
    {
        if (_menuItems[i]->id() == id)
        {
            return _menuItems[i];
        }
    }
    return NULL;
}

// Add a new menu item to the list (order matters)
void LcdMenu::addItem(const char *disp, byte id)
{
    _menuItems[_numMenuItems++] = new MenuItem(disp, id);
    _longestDisplay             = max((size_t) _longestDisplay, strlen(disp));
}

// Get the currently active item ID
byte LcdMenu::getActive()
{
    return _menuItems[_activeMenuIndex]->id();
}

// Set the active menu item
void LcdMenu::setActive(byte id)
{
    for (byte i = 0; i < _numMenuItems; i++)
    {
        if (_menuItems[i]->id() == id)
        {
            _activeMenuIndex = i;
            break;
        }
    }
}

// Pass thru utility function
void LcdMenu::setCursor(byte col, byte row)
{
    _activeRow = row;
    _activeCol = col;
}

// Pass thru utility function
void LcdMenu::clear()
{
    _lcd.clear();
}

// Set the brightness of the backlight
void LcdMenu::setBacklightBrightness(int level, bool persist)
{
    _brightness = level;

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD && defined(LCD_BRIGHTNESS_PIN)
    // Not supported on ESP32 due to lack of built-in analogWrite()
    if (_lcdBadHw)
    {
        // On 'bad' hardware you can only turn off or on
        if (_brightness > 0)
        {
            pinMode(LCD_BRIGHTNESS_PIN, INPUT);
        }
        else
        {
            pinMode(LCD_BRIGHTNESS_PIN, OUTPUT);
        }
    }
    else
    {
        analogWrite(LCD_BRIGHTNESS_PIN, _brightness);
    }
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
        // Nothing to do?
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    _lcd.setContrast(_brightness);
    #endif

    if (persist)
    {
        LOG(DEBUG_INFO, "[LCD]: Saving %d as brightness", _brightness);
        EEPROMStore::storeBrightness(_brightness);
    }
}

// Get the current brightness
int LcdMenu::getBacklightBrightness() const
{
    return _brightness;
}

void LcdMenu::getBacklightBrightnessRange(int *minPtr, int *maxPtr) const
{
    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD
    if (_lcdBadHw)
    {
        // Bad LCD displays are either on or off
        *minPtr = 0;
        *maxPtr = 1;
    }
    else
    #endif
    {
        // Full range otherwise
        *minPtr = 0;
        *maxPtr = 255;
    }
}

// Go to the next menu item from currently active one
void LcdMenu::setNextActive()
{
    _activeMenuIndex = adjustWrap(_activeMenuIndex, 1, 0, _numMenuItems - 1);

    // Update the display
    updateDisplay();

    // Clear submenu line, in case new menu doesn't print anything.
    _lcd.setCursor(0, 1 * _charHeightRows);
    for (byte i = 0; i < _columns; i++)
    {
        _lcd.print(" ");
    }
}

// Update the display of the LCD with the current menu settings
// This iterates over the menu items, building a menu string by concatenating their display string.
// It also places the selector arrows around the active one.
// It then sends the string to the LCD, keeping the selector arrows centered in the same place.
void LcdMenu::updateDisplay()
{
    char bufMenu[17];
    char *pBufMenu      = &bufMenu[0];
    String menuString   = "";
    byte offsetToActive = 0;
    byte offset         = 0;

    char scratchBuffer[12];
    // Build the entire menu string
    for (byte i = 0; i < _numMenuItems; i++)
    {
        MenuItem *item = _menuItems[i];
        bool isActive  = i == _activeMenuIndex;
        sprintf(scratchBuffer, "%c%s%c", isActive ? '>' : ' ', item->display(), isActive ? '<' : ' ');

        // For the active item remember where it starts in the string and insert selector arrows
        offsetToActive = isActive ? offset : offsetToActive;
        menuString += String(scratchBuffer);
        offset += strlen(scratchBuffer);
    }

    _lcd.setCursor(0, 0);
    _activeRow        = 0;
    _activeCol        = 0;
    int usableColumns = _columns - 1;  // Leave off last one to have distance to tracking indicator

    // Determine where to place the active menu item. (empty space around longest item divided by two).
    int margin           = (usableColumns - (_longestDisplay)) / 2;
    int offsetIntoString = offsetToActive - margin;

    // Pad the front if we don't have enough to offset the string to the arrow locations (happens on first item(s))
    while (offsetIntoString < 0)
    {
        *(pBufMenu++) = ' ';
        offsetIntoString++;
    }

    // Display the actual menu string
    while ((pBufMenu < bufMenu + usableColumns) && (offsetIntoString < (int) menuString.length()))
    {
        *(pBufMenu++) = menuString[offsetIntoString++];
    }

    // Pad the end with spaces so the display is cleared when getting to the last item(s).
    while (pBufMenu < bufMenu + _columns)
    {
        *(pBufMenu++) = ' ';
    }
    *(pBufMenu++) = 0;

    printMenu(String(bufMenu));

    setCursor(0, 1);
}

// Print the given character to the LCD, converting some special ones to our bitmaps
void LcdMenu::printChar(char ch)
{
    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    struct charData_t {
        const uint8_t *font;
        uint8_t encoding;
    };

    MappedDict<char, charData_t>::DictEntry_t lookupTable[] = {
        {'>', {.font = u8x8_font_open_iconic_arrow_1x1, .encoding = 64 + 14}},  // Right arrow
        {'<', {.font = u8x8_font_open_iconic_arrow_1x1, .encoding = 64 + 13}},  // Left arrow
        {'^', {.font = u8x8_font_open_iconic_arrow_1x1, .encoding = 64 + 15}},  // Up arrow
        {'~', {.font = u8x8_font_open_iconic_arrow_1x1, .encoding = 64 + 12}},  // Down arrow
        {'@', {.font = u8x8_font_7x14_1x2_f, .encoding = 176}},                 // Degrees
        {'&', {.font = u8x8_font_open_iconic_thing_1x1, .encoding = 64 + 15}},  // Tracking
        {'`', {.font = u8x8_font_open_iconic_thing_1x1, .encoding = 64 + 4}},   // Not tracking
    };
    auto buttonLookup      = MappedDict<char, charData_t>(lookupTable, ARRAY_SIZE(lookupTable));
    charData_t specialChar = {};
    const bool charInTable = buttonLookup.tryGet(ch, &specialChar);
    if (charInTable)
    {
        _lcd.setFont(specialChar.font);
        _lcd.drawGlyph(_lcd.tx, _lcd.ty, specialChar.encoding);
    }
    else
    {
        _lcd.setFont(u8x8_font_7x14_1x2_f);
        _lcd.drawGlyph(_lcd.tx, _lcd.ty, ch);
    }

    _lcd.tx += 1;
    #else
    MappedDict<char, specialChar_t>::DictEntry_t lookupTable[] = {
        {'>', _rightArrow},
        {'<', _leftArrow},
        {'^', _upArrow},
        {'~', _downArrow},
        {'@', _degrees},
        {'\'', _minutes},
        {'&', _tracking},
        {'`', _noTracking},
    };
    auto buttonLookup = MappedDict<char, specialChar_t>(lookupTable, ARRAY_SIZE(lookupTable));
    specialChar_t specialChar;
    const bool charInTable = buttonLookup.tryGet(ch, &specialChar);
    if (charInTable)
    {
        _lcd.write(specialChar);
    }
    else
    {
        _lcd.print(ch);
    }
    #endif
}

// Print a character at a specific position
void LcdMenu::printAt(int col, int row, char ch)
{
    _lcd.setCursor(col, _charHeightRows * row);
    printChar(ch);
}

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
uint8_t LcdMenu::readButtons()
{
    return _lcd.readButtons();
}
    #endif

// Print a string to the LCD at the current cursor position, substituting the special arrows and padding with spaces to the end
void LcdMenu::printMenu(String line)
{
    if ((_lastDisplay[_activeRow] != line) || (_activeCol != 0))
    {
        _lastDisplay[_activeRow] = line;

        _lcd.setCursor(_activeCol, _charHeightRows * _activeRow);
        int spaces = _columns - line.length();
        for (char i : line)
        {
            printChar(i);
        }

        // Clear the rest of the display
        while (spaces > 0)
        {
            _lcd.print(" ");
            spaces--;
        }
    }
}

    #if DISPLAY_TYPE != DISPLAY_TYPE_LCD_JOY_I2C_SSD1306

// The right arrow bitmap
byte LcdMenu::RightArrowBitmap[8] = {B00000, B01000, B01100, B01110, B01100, B01000, B00000, B00000};

// The left arrow bitmap
byte LcdMenu::LeftArrowBitmap[8] = {B00000, B00010, B00110, B01110, B00110, B00010, B00000, B00000};

byte LcdMenu::UpArrowBitmap[8] = {B00100, B01110, B11111, B00100, B00100, B00100, B00100, B00100};

byte LcdMenu::DownArrowBitmap[8] = {B000100, B000100, B000100, B000100, B000100, B011111, B001110, B000100};

byte LcdMenu::DegreesBitmap[8] = {B01100, B10010, B10010, B01100, B00000, B00000, B00000, B00000};

byte LcdMenu::MinutesBitmap[8] = {B01000, B01000, B01000, B00000, B00000, B00000, B00000, B00000};

byte LcdMenu::TrackingBitmap[8] = {B10111, B00010, B10010, B00010, B10111, B00101, B10110, B00101};

byte LcdMenu::NoTrackingBitmap[8] = {B10000, B00000, B10000, B00010, B10000, B00000, B10000, B00000};

    #endif

#else  // Headless (i.e. DISPLAY_TYPE == 0)

LcdMenu::LcdMenu(byte cols, byte rows, int maxItems)
{
}

MenuItem *LcdMenu::findById(byte id)
{
    return NULL;
}

void LcdMenu::addItem(const char *disp, byte id)
{
}

byte LcdMenu::getActive()
{
    return 0;
}

void LcdMenu::setActive(byte id)
{
}

void LcdMenu::setCursor(byte col, byte row)
{
}

void LcdMenu::clear()
{
}

void LcdMenu::setNextActive()
{
}

void LcdMenu::updateDisplay()
{
}

void LcdMenu::printMenu(String line)
{
}

void LcdMenu::printChar(char ch)
{
}

void LcdMenu::printAt(int col, int row, char ch)
{
}

#endif
