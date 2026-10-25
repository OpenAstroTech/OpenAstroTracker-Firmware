#include "LcdButtons.hpp"

#if DISPLAY_TYPE > 0

    LcdButtons::LcdButtons(ports::IButtons *buttons) : _buttons(buttons)
    {
    }

    lcdButton_t LcdButtons::currentKey()
    {
        return toLcd(_buttons != nullptr ? _buttons->pressed() : core::Button::None);
    }

    int LcdButtons::currentAnalogState()
    {
        return (_buttons != nullptr) ? static_cast<int>(_buttons->rawValue()) : 0;
    }

    bool LcdButtons::keyChanged(lcdButton_t *pNewKey)
    {
        if (_buttons == nullptr)
        {
            return false;
        }

        core::Button key;
        if (!_buttons->keyChanged(&key))
        {
            return false;
        }

        if (pNewKey != nullptr)
        {
            *pNewKey = toLcd(key);
        }
        return true;
    }

    core::Button toCoreButton(lcdButton_t key)
{
    switch (key)
    {
        case btnRIGHT:
            return core::Button::Right;
        case btnUP:
            return core::Button::Up;
        case btnDOWN:
            return core::Button::Down;
        case btnLEFT:
            return core::Button::Left;
        case btnSELECT:
            return core::Button::Select;
        case btnINVALID:
            return core::Button::Invalid;
        case btnNONE:
        default:
            return core::Button::None;
    }
}

lcdButton_t LcdButtons::toLcd(core::Button key)
    {
        switch (key)
        {
            case core::Button::Right:
                return btnRIGHT;
            case core::Button::Up:
                return btnUP;
            case core::Button::Down:
                return btnDOWN;
            case core::Button::Left:
                return btnLEFT;
            case core::Button::Select:
                return btnSELECT;
            case core::Button::Invalid:
                return btnINVALID;
            case core::Button::None:
            default:
                return btnNONE;
        }
    }

#else  // Headless boards have no keys.

LcdButtons::LcdButtons(ports::IButtons *buttons)
{
    (void) buttons;
}

lcdButton_t LcdButtons::currentKey()
{
    return btnNONE;
}

int LcdButtons::currentAnalogState()
{
    return 0;
}

bool LcdButtons::keyChanged(lcdButton_t *pNewKey)
{
    (void) pNewKey;
    return false;
}

core::Button toCoreButton(lcdButton_t key)
{
    switch (key)
    {
        case btnRIGHT:
            return core::Button::Right;
        case btnUP:
            return core::Button::Up;
        case btnDOWN:
            return core::Button::Down;
        case btnLEFT:
            return core::Button::Left;
        case btnSELECT:
            return core::Button::Select;
        case btnINVALID:
            return core::Button::Invalid;
        case btnNONE:
        default:
            return core::Button::None;
    }
}

lcdButton_t LcdButtons::toLcd(core::Button key)
{
    (void) key;
    return btnNONE;
}

#endif
