#ifndef SWEEP_H_NOS
#define SWEEP_H_NOS

#include <cstdint>

namespace NES
{

struct Sweep
{
    bool pulse_fst_snd;
    bool should_reload;
    bool enabled;
    bool negate;
    uint8_t div_ctr : 3;
    uint8_t div_reload : 3;
    uint8_t shamt : 3;
    uint16_t target_reload : 11;
    bool sweep_overflow;
    uint16_t& timer_clock_reload;

    // Invoke any time target_reload might change value
    void set_target_reload()
    {
        uint16_t raw_reload = timer_clock_reload;
        uint16_t change_amt = raw_reload >> shamt;

        if(negate) 
            change_amt = -change_amt - (pulse_fst_snd ? 1 : 0);

        uint16_t target_reload_new = raw_reload + change_amt;
        sweep_overflow = (target_reload_new & (0xFFFFU << 11));
        
        target_reload = target_reload_new;
    }

    Sweep(uint16_t& timer_clock_reload, bool pulse_fst_snd) 
        : timer_clock_reload(timer_clock_reload), pulse_fst_snd(pulse_fst_snd) 
    {
    }

    void tick_frame_half()
    {
        if(div_ctr == 0 && enabled && is_audible() && (shamt > 0))
        {
            timer_clock_reload = target_reload;
            set_target_reload();
        }
        
        if((div_ctr == 0) || should_reload)
        {
            div_ctr = div_reload;
            should_reload = false;
        }
        else
        {
            --div_ctr;
        }
    }

    void write_b(uint8_t data)
    {
        should_reload = true;

        enabled    = (data >> 7) & 1U;
        div_reload = (data >> 4) & (0xFU >> 1);
        negate     = (data >> 3) & 1U;
        shamt      = (data >> 0) & (0xFU >> 1);

        set_target_reload();
    }

    void write_c(uint8_t data) 
    { 
        set_target_reload();
    }

    void write_d(uint8_t data) 
    { 
        set_target_reload();
    }

    bool is_audible()
    {
        return (!sweep_overflow && (timer_clock_reload >= 8));
    }
};


}

#endif //SWEEP_H_NOS
