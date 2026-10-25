#pragma once

#include "../Configuration.hpp"
#include "core/Button.hpp"
#include "ports/IButtons.hpp"

// The screens' view of the keys.
//
// The panel is behind `ports::IButtons`, which already debounces; this enum is the vocabulary the
// screen headers still use (`btnUP`, `btnSELECT`, …) and disappears with them (PLAN.md 8.8).
enum lcdButton_t
{
    btnRIGHT,
    btnUP,
    btnDOWN,
    btnLEFT,
    btnSELECT,
    btnNONE,
    btnINVALID,
};

class LcdButtons
{
  public:
    explicit LcdButtons(ports::IButtons *buttons);

    // The key held now, once it has settled
    lcdButton_t currentKey();

    // The same key in the legacy spelling; kept because the screens call both
    lcdButton_t currentState()
    {
        return currentKey();
    }

    // Raw reading of the resistive ladder, for the key diagnostic screen
    int currentAnalogState();

    // True once per press or release of the settled key
    bool keyChanged(lcdButton_t *pNewKey);

  private:
    static lcdButton_t toLcd(core::Button key);

#if DISPLAY_TYPE > 0
    ports::IButtons *_buttons;
#endif
};
