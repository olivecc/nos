#ifndef  PT_TIMER_H_NOS
#define  PT_TIMER_H_NOS

#include <cstdint>

namespace NES
{


// For use with Pulse and Triangle channels
struct PT_Timer
{
    uint16_t clock_reload;
    uint16_t clock : 11;

    void write_c(uint8_t data)
    {
        clock_reload &= 0xFF00U;
        clock_reload |= data;
    }

    void write_d(uint8_t data)
    {
        clock_reload &= 0x00FFU;
        clock_reload |= (data << 8);
        clock_reload &= ~(0xFFFFU << 11);
    }

    bool pulse_clock()
    {
        bool pulse_seq = false;

        if(clock > 0)
        {
            --clock;
        }
        else
        {
            pulse_seq = true;
            clock = clock_reload;
        }

        return pulse_seq;
    }
};


}

#endif //PT_TIMER_H_NOS
