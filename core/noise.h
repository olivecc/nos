#ifndef  NOISE_H_NOS
#define  NOISE_H_NOS

#include "n_timer.h"
#include "lectr.h"
#include "envel.h"

namespace NES
{


class Noise
{
  private:
    N_Timer timer;
    Length_Counter lectr;
    Envelope envel;

    uint16_t shift_reg : 15;
    bool mode;

  public:
    Noise() : envel(lectr.halt), shift_reg(1U) {}

    void set_enabled(bool val) { lectr.set_enabled(val); }
    bool is_active() { return lectr.is_active(); }

    void write_a(uint8_t data)
    {
        envel.write_a(data);
    }

    void write_b(uint8_t data)
    {
        // No effect
    }

    void write_c(uint8_t data)
    {
        mode = data & (1U << 7);

        timer.write_c(data);
    }

    void write_d(uint8_t data)
    {
        lectr.write_d(data);
        envel.write_d(data);
    }

    void tick()
    {
        bool pulse_shift_reg = timer.pulse_clock();
        if(pulse_shift_reg)
        {
            unsigned int xor_bit = (mode ? 6 : 1);
            bool feedback = ((shift_reg >>       0) & 1U) ^ 
                            ((shift_reg >> xor_bit) & 1U);
            shift_reg >>= 1;
            shift_reg |= ((feedback ? 1U : 0U) << 14);

        }
    }

    void tick_frame_quarter()
    {
        envel.tick_frame_quarter();
    }

    void tick_frame_half()
    {
        lectr.tick_frame_half();
    }

    uint8_t vol()
    {
        return ((((shift_reg & (1U << 0)) == 0) && (lectr.is_active()))
            ? envel.vol()
            : 0);
    }
};


}

#endif //NOISE_H_NOS
