#ifndef LCDBUTTONS_HPP_
#define LCDBUTTONS_HPP_

#include "inc/Globals.hpp"

// LCD shield buttons
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Forward declaration
class LcdMenu;

class LcdButtons {
public:
  LcdButtons(byte pin, LcdMenu* lcdMenu);
  LcdButtons(LcdMenu* lcdMenu);
  
  byte currentKey() {
    checkKey();
    return _newKey;
  }

  byte currentState() {
    checkKey();
    return _currentKey;
  }

  int currentAnalogState() {
    checkKey();
    return _analogKeyValue;
  }

  bool keyChanged(byte* pNewKey);

private:
  void checkKey();
    
private:

  unsigned long _lastKeyChange;
  byte _analogPin;
  int _analogKeyValue;
  byte _lastKey;
  byte _newKey;
  byte _lastNewKey;
  byte _currentKey;
  LcdMenu* _lcdMenu;
};

#endif // LCDBUTTONS_HPP_