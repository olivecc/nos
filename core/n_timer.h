#ifndef  N_TIMER_H_NOS
#define  N_TIMER_H_NOS

#include <cstdint>

namespace NES
{


// For use with Noise channel
struct N_Timer
{
    static constexpr uint16_t clock_table[0x10] = 
    {
        0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0060, 0x0080, 0x00A0, 
        0x00CA, 0x00FE, 0x017C, 0x01FC, 0x02FA, 0x03F8, 0x07F2, 0x0FE4
    };

    uint16_t clock_reload;
    uint16_t clock;

    void write_c(uint8_t data)
    {
        uint8_t reload_index = data & 0xFU;
        clock_reload = clock_table[reload_index];
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

#endif //N_TIMER_H_NOS
