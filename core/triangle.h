#ifndef  TRIANGLE_H_NOS
#define  TRIANGLE_H_NOS

#include <cstdint>

#include "pt_timer.h"
#include "lectr.h"
#include "lictr.h"

namespace NES
{


class Triangle
{
  private:
    PT_Timer timer;
    Length_Counter lectr;
    Linear_Counter lictr;

    uint8_t seq : 5;

  public:
    Triangle() : lictr(lectr.halt) {}

    void set_enabled(bool val) { lectr.set_enabled(val); }
    bool is_active() { return lectr.is_active(); }
    
    void write_a(uint8_t data)
    {
        lictr.write_a(data);
    }

    void write_b(uint8_t data)
    {
        // No effect
    }

    void write_c(uint8_t data)
    {
        timer.write_c(data);
    }

    void write_d(uint8_t data)
    {
        timer.write_d(data);
        lectr.write_d(data);
        lictr.write_d(data);
    }
    
    void tick()
    {
        bool pulse_seq = timer.pulse_clock();
        if(pulse_seq && lectr.is_active() && lictr.is_active()) ++seq;
    }

    void tick_frame_quarter()
    {
        lictr.tick_frame_quarter();
    }

    void tick_frame_half()
    {
        lectr.tick_frame_half();
    }

    uint8_t vol()
    {
        bool vol_asc_desc = seq & (1U << 4);
        uint8_t volume = (seq ^ ((vol_asc_desc) ? 0x0U : 0xFU)) & 0xFU;
        return volume;
    }

};


}

#endif //TRIANGLE_H_NOS
