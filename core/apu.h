#ifndef  APU_H_NOS
#define  APU_H_NOS

#include <cstdint>

#include "pulse.h"
#include "triangle.h"
#include "noise.h"

namespace NES
{

class APU
{
  private:
    Shared_Bus& shared_bus;

    Pulse pulse_fst;
    Pulse pulse_snd;
    Triangle triangle;
    Noise noise;

    bool frame_surpress_irq;
    bool frame_seq_alt_mode;
    uint32_t frame_div_ctr;
    uint8_t frame_seq;
    float lookup_pulse_out[0x1F];
    float lookup_tnd_out[0x10][0x10][0x80];

    void tick_frame_quarter()
    {
        pulse_fst.tick_frame_quarter();
        pulse_snd.tick_frame_quarter();
         triangle.tick_frame_quarter();
            noise.tick_frame_quarter();
    }

    void tick_frame_half()
    {
        pulse_fst.tick_frame_half();
        pulse_snd.tick_frame_half();
         triangle.tick_frame_half();
            noise.tick_frame_half();
    }

  public:
    APU(Shared_Bus& shared_bus) 
        : shared_bus(shared_bus), pulse_fst(true), pulse_snd(false)
    {
        // Populate mixer lookup tables
        for(unsigned int pulse_sum = 1; pulse_sum < 0x1F; ++pulse_sum)
        {
            double val = ((pulse_sum > 0) 
                ? (95.88 / (100 + (8128.0 / pulse_sum)))
                : 0);
            lookup_pulse_out[pulse_sum] = val;
        }

        for(unsigned int triangle = 0; triangle < 0x10; ++triangle)
        {
            for(unsigned int noise = 0; noise < 0x10; ++noise)
            {
                for(unsigned int dmc = 0; dmc < 0x80; ++dmc)
                {
                    unsigned int sum = triangle + noise + dmc;
                    double val = ((sum > 0)
                        ? (159.79 / (100 + (1 / ((triangle / 8227.0) +
                                                 (noise   / 12241.0) +
                                                 (dmc     / 22638.0)))))
                        : 0);
                    lookup_tnd_out[triangle][noise][dmc] = val;
                }
            }
        }
    }

    void process_frame_cpu_phase()
    {
        constexpr unsigned int frame_div_period = 89490;
        constexpr unsigned int master_cycles_per_cpu_phase = 6;
        frame_div_ctr += master_cycles_per_cpu_phase;

        if(frame_div_ctr >= frame_div_period)
        {
            frame_div_ctr -= frame_div_period;

            if(frame_seq < 4)
            {
                tick_frame_quarter();

                if(frame_seq % 2 == (frame_seq_alt_mode ? 0 : 1))
                    tick_frame_half();
                
                if(frame_seq == 3 && !frame_seq_alt_mode && !frame_surpress_irq)
                    shared_bus.line_irq_low |= IRQ_Src::APU_FRAME;
            }

            unsigned int step_num = (frame_seq_alt_mode ? 5 : 4);
            ++frame_seq;
            frame_seq %= step_num;
        }
    }

    void tick(bool is_odd_cycle)
    {
        if(is_odd_cycle)
        {
            pulse_fst.tick();
            pulse_snd.tick();
                noise.tick();
        }

        triangle.tick();

        uint8_t pulse_vol = pulse_fst.vol() + pulse_snd.vol();
        float pulse_out = lookup_pulse_out[pulse_vol];
        // TODO dmc
        float tnd_out   = lookup_tnd_out[triangle.vol()][noise.vol()][0]; 
        float output = pulse_out + tnd_out;
        shared_bus.audiobuf.push(output);
    }

    // Precondition: sub_addr < 4
    void write_reg_pulse(uint8_t sub_addr, uint8_t data, bool pulse_fst_snd)
    {
        Pulse& pulse = (pulse_fst_snd ? pulse_fst : pulse_snd);
        switch(sub_addr)
        {
            case(0): pulse.write_a(data); break;
            case(1): pulse.write_b(data); break;
            case(2): pulse.write_c(data); break;
            case(3): pulse.write_d(data); break;
            default: break;
        }
    }

    // Precondition: sub_addr < 4
    void write_reg_triangle(uint8_t sub_addr, uint8_t data)
    {
        switch(sub_addr)
        {
            case(0): triangle.write_a(data); break;
            case(1): triangle.write_b(data); break;
            case(2): triangle.write_c(data); break;
            case(3): triangle.write_d(data); break;
            default: break;
        }
    }

    void write_reg_noise(uint8_t sub_addr, uint8_t data)
    {
        switch(sub_addr)
        {
            case(0): noise.write_a(data); break;
            case(1): noise.write_b(data); break;
            case(2): noise.write_c(data); break;
            case(3): noise.write_d(data); break;
            default: break;
        }
    }

    uint8_t read_reg_status()
    {
        const bool irq_frame = shared_bus.line_irq_low & IRQ_Src::APU_FRAME;
        const bool irq_dmc   = shared_bus.line_irq_low & IRQ_Src::APU_DMC;

        uint8_t value = (((pulse_fst.is_active()  ? 1U : 0U) << 0) |
                         ((pulse_snd.is_active()  ? 1U : 0U) << 1) |
                         (( triangle.is_active()  ? 1U : 0U) << 2) |
                         ((    noise.is_active()  ? 1U : 0U) << 3) |
                         ((irq_frame              ? 1U : 0U) << 6) |
                         ((irq_dmc                ? 1U : 0U) << 7));

        shared_bus.line_irq_low &= ~(IRQ_Src::APU_FRAME);

        return value;
    }

    void write_reg_status(uint8_t data)
    {
        pulse_fst.set_enabled(data & (1U << 0));
        pulse_snd.set_enabled(data & (1U << 1));
         triangle.set_enabled(data & (1U << 2));
            noise.set_enabled(data & (1U << 3));

        shared_bus.line_irq_low &= ~(IRQ_Src::APU_DMC);
    }

    void write_reg_frame(uint8_t data)
    {
        frame_surpress_irq = (data & (1U << 6));
        frame_seq_alt_mode = (data & (1U << 7));
        if(frame_surpress_irq) 
            shared_bus.line_irq_low &= ~(IRQ_Src::APU_FRAME);
        // Should really be delayed by (odd/even:3/5) phases
        frame_div_ctr = 0;
        frame_seq = 0;
        if(frame_seq_alt_mode)
        {
            tick_frame_quarter();
            tick_frame_half();
        }
    }
};

}

#endif //APU_H_NOS
