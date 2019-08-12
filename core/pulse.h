#ifndef  PULSE_H_NOS
#define  PULSE_H_NOS

#include <cstdint>

#include "lectr.h"
#include "envel.h"
#include "pt_timer.h"
#include "sweep.h"

namespace NES
{


struct Pulse
{
    Length_Counter lectr;
    Envelope envel;
    PT_Timer timer;
    Sweep sweep;
        
    static constexpr uint8_t duty_table[4] = 
    {
        ~(0xFFU >> 1) & 0xFF,
        ~(0xFFU >> 2) & 0xFF,
        ~(0xFFU >> 4) & 0xFF, 
         (0xFFU >> 2) & 0xFF
    };

    uint8_t seq : 3;
    uint8_t duty_index : 2;

    Pulse(bool fst_snd) 
        : envel(lectr.halt), sweep(timer.clock_reload, fst_snd) 
    {
    }

    void set_enabled(bool val) { lectr.set_enabled(val); }
    bool is_active() { return lectr.is_active(); }

    void write_a(uint8_t data)
    {
        envel.write_a(data);
        duty_index = (data >> 6);
    }

    void write_b(uint8_t data)
    {
        sweep.write_b(data);
    }

    void write_c(uint8_t data)
    {
        timer.write_c(data);
        sweep.write_c(data);
    }

    void write_d(uint8_t data)
    {
        seq = 0;

        lectr.write_d(data);
        envel.write_d(data);
        timer.write_d(data);
        sweep.write_d(data);
    }

    void tick()
    {
        bool pulse_seq = timer.pulse_clock();
        if(pulse_seq) --seq;
    }

    void tick_frame_quarter()
    {
        envel.tick_frame_quarter();
    }

    void tick_frame_half()
    {
        lectr.tick_frame_half();
        sweep.tick_frame_half();
    }

    uint8_t vol()
    {
        uint8_t waveform = duty_table[duty_index];
        bool is_waveform_high = ((waveform >> seq) & 1U);

        return ((is_waveform_high && sweep.is_audible() && lectr.is_active()) 
            ? envel.vol() 
            : 0);
    }
};


}

#endif //PULSE_H_NOS
