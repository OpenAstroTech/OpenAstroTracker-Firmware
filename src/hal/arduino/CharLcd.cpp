#include "hal/arduino/CharLcd.hpp"

#include "../../../Configuration.hpp"

// A board without a menu panel compiles none of this: the panel backends are the only place the
// display libraries are named, so leaving them out keeps a headless build free of them.
#if DISPLAY_TYPE > 0

    #include <Arduino.h>
    #include <string.h>

namespace hal
{

CharLcd::CharLcd(uint8_t rs, uint8_t enable, uint8_t d4, uint8_t d5, uint8_t d6, uint8_t d7, IGpioPin *backlightPin, bool backlightShorted)
    : _lcd(rs, enable, d4, d5, d6, d7), _backlightPin(backlightPin), _backlightShorted(backlightShorted)
{
}

bool CharLcd::begin(uint8_t columns, uint8_t rows)
{
    _lcd.begin(columns, rows);
    return true;
}

void CharLcd::clear()
{
    _lcd.clear();
}

void CharLcd::setCursor(uint8_t column, uint8_t row)
{
    _lcd.setCursor(column, row);
}

void CharLcd::print(const char *text)
{
    _lcd.print(text);
}

void CharLcd::write(uint8_t value)
{
    _lcd.write(value);
}

bool CharLcd::hasDefinableSymbols() const
{
    return true;
}

void CharLcd::defineSymbol(CharSymbol symbol, const uint8_t bitmap[8])
{
    // LiquidCrystal wants a mutable buffer and keeps the pointer, so the copy has to outlive the call.
    static uint8_t slots[8][8];
    const uint8_t slot = static_cast<uint8_t>(symbol);
    memcpy(slots[slot], bitmap, 8);
    _lcd.createChar(slot, slots[slot]);
}

void CharLcd::writeSymbol(CharSymbol symbol)
{
    _lcd.write(static_cast<uint8_t>(symbol));
}

void CharLcd::setBacklightLevel(uint8_t level)
{
    if (_backlightPin == nullptr)
    {
        return;
    }

    if (_backlightShorted)
    {
        // On the broken shield the pin may only float (backlight on) or sink (backlight off).
        _backlightPin->configure((level > 0) ? PinMode::Input : PinMode::Output);
        return;
    }

    _backlightPin->writeAnalog(level);
}

bool backlightPinIsShorted(uint8_t pin)
{
    // Drive low, release the pullup, then drive high: on a correct shield the pin reads back high,
    // on the shorted one the transistor pulls it down.
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    delayMicroseconds(5);
    const int level = digitalRead(pin);

    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);

    return level != HIGH;
}

}  // namespace hal

#endif
