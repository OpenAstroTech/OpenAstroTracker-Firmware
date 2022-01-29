#include "inc/Globals.hpp"
#include "../Configuration.hpp"
#include "LcdMenu.hpp"
#include "LcdButtons.hpp"

#if DISPLAY_TYPE != DISPLAY_TYPE_NONE

LcdButtons::LcdButtons(byte pin, LcdMenu *lcdMenu)
{
    _lcdMenu       = lcdMenu;
    _analogPin     = pin;
    _lastKeyChange = 0;

    _newKey     = btnNONE;
    _lastNewKey = btnNONE;

    _currentKey = btnNONE;
    _lastKey    = btnNONE;

    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    // Initialize keypad
    pinMode(LCD_KEY_SENSE_X_PIN, INPUT);
    pinMode(LCD_KEY_SENSE_Y_PIN, INPUT);
    pinMode(LCD_KEY_SENSE_PUSH_PIN, INPUT);
    #endif
}

LcdButtons::LcdButtons(LcdMenu *lcdMenu)
{
    _lcdMenu       = lcdMenu;
    _lastKeyChange = 0;

    _newKey     = btnNONE;
    _lastNewKey = btnINVALID;

    _currentKey = btnNONE;
    _lastKey    = btnINVALID;
}

bool LcdButtons::keyChanged(lcdButton_t *pNewKey)
{
    checkKey();
    if (_newKey != _lastNewKey)
    {
        *pNewKey    = _newKey;
        _lastNewKey = _newKey;
        return true;
    }
    return false;
}

int LcdButtons::currentAnalogState()
{
    #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    return 0;  // No analog value for these displays
    #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    return analogRead(LCD_KEY_SENSE_Y_PIN);
    #else
    return analogRead(_analogPin);
    #endif
}

void LcdButtons::checkKey()
{
    #if DISPLAY_TYPE > 0
        #if DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23008 || DISPLAY_TYPE == DISPLAY_TYPE_LCD_KEYPAD_I2C_MCP23017
    uint8_t buttons = _lcdMenu->readButtons();
    _currentKey     = btnNONE;
    if (buttons)
    {
        if (buttons & BUTTON_UP)
            _currentKey = btnUP;
        if (buttons & BUTTON_DOWN)
            _currentKey = btnDOWN;
        if (buttons & BUTTON_LEFT)
            _currentKey = btnLEFT;
        if (buttons & BUTTON_RIGHT)
            _currentKey = btnRIGHT;
        if (buttons & BUTTON_SELECT)
            _currentKey = btnSELECT;
    }
        #elif DISPLAY_TYPE == DISPLAY_TYPE_LCD_JOY_I2C_SSD1306
    uint16_t x(analogRead(LCD_KEY_SENSE_X_PIN));
    uint16_t y(analogRead(LCD_KEY_SENSE_Y_PIN));
    uint16_t push(analogRead(LCD_KEY_SENSE_PUSH_PIN));

    // Assumes analogReadResolution(12) (the default)
    int16_t const MIDSCALE = 4096 / 2;
    int16_t const DEADBAND = 500;

    _currentKey = btnNONE;
    if (x > (MIDSCALE + DEADBAND))
        _currentKey = btnRIGHT;
    if (x < (MIDSCALE - DEADBAND))
        _currentKey = btnLEFT;
    if (y > (MIDSCALE + DEADBAND))
        _currentKey = btnDOWN;  // Y appears reversed
    if (y < (MIDSCALE - DEADBAND))
        _currentKey = btnUP;
    if (push < MIDSCALE)
        _currentKey = btnSELECT;  // Active low
        #else
    const int analogKeyValue = currentAnalogState();
    if (analogKeyValue > 1000)
        _currentKey = btnNONE;
    else if (analogKeyValue < 50)
        _currentKey = btnRIGHT;
    else if (analogKeyValue < 240)
        _currentKey = btnUP;
    else if (analogKeyValue < 400)
        _currentKey = btnDOWN;
    else if (analogKeyValue < 600)
        _currentKey = btnLEFT;
    else if (analogKeyValue < 920)
        _currentKey = btnSELECT;
        #endif

    if (_currentKey != _lastKey)
    {
        _lastKey       = _currentKey;
        _lastKeyChange = millis();
    }
    else
    {
        // If the keys haven't changed in 5ms, commit the change to the new keys.
        if (millis() - _lastKeyChange > 5)
        {
            _newKey = _currentKey;
        }
    }
    #endif
}

#else

// Null implementation
LcdButtons::LcdButtons(byte pin, LcdMenu *lcdMenu)
    : _lastKeyChange(0), _analogPin(pin), _lastKey(btnNONE), _newKey(btnNONE), _lastNewKey(btnNONE), _currentKey(btnNONE), _lcdMenu(lcdMenu)
{
}

LcdButtons::LcdButtons(LcdMenu *lcdMenu)
    : _lastKeyChange(0), _analogPin(0), _lastKey(btnNONE), _newKey(btnNONE), _lastNewKey(btnNONE), _currentKey(btnNONE), _lcdMenu(lcdMenu)
{
}

bool LcdButtons::keyChanged(lcdButton_t *pNewKey)
{
    return false;
}

void LcdButtons::checkKey()
{
}

#endif