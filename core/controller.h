#ifndef  CONTROLLER_H_NOS
#define  CONTROLLER_H_NOS

#include <cstdint>

namespace NES
{


// Unsure of the mechanism by which reading through 0x4016/0x4017 more than
// eight times returns 1, but this matches the behaviour efficiently
class Controller
{
  private:
    bool strobe = false;
    uint8_t pad_held_state = 0xFF;
    uint8_t pad_true_state = 0x00;
    
    void refresh() { pad_held_state = ~pad_true_state; }

  public:
    enum class Button : unsigned int
    {
        A,
        B,
        SELECT,
        START,
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    void set_state(Button btn, bool is_pressed)
    {
        unsigned int shamt = static_cast<unsigned int>(btn);
        pad_true_state &= ~(1U << shamt);
        pad_true_state |= ((is_pressed ? 1U : 0) << shamt);
    }

    void set_strobe(bool value) 
    { 
        strobe = value;
        if(strobe) refresh();
    }

    uint8_t read_bit()
    {
        if(strobe) refresh();
        uint8_t bit = ~pad_held_state & (1U << 0);
        if(!strobe) pad_held_state >>= 1;
        return bit;
    }
};


}

#endif //CONTROLLER_H_NOS
