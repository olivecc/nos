#ifndef  ENVEL_H_NOS
#define  ENVEL_H_NOS

namespace NES
{


class Envelope
{
  private:
    bool start;
    bool use_const_vol;
    uint8_t const_vol : 4;
    uint8_t div_ctr : 4;
    uint8_t decay_lvl_ctr : 4;
    bool& lectr_halt;

  public:
    Envelope(bool& lectr_halt) : lectr_halt(lectr_halt) {}

    void write_a(uint8_t data)
    {
        lectr_halt  = (data & (1U << 5));
        use_const_vol = (data & (1U << 4));
        const_vol     = (data & 0xFU);
    }

    void write_d(uint8_t data)
    {
        start = true;
    }

    void tick_frame_quarter()
    {
        if(start)
        {
            start = false;
            decay_lvl_ctr = 0xF;
            div_ctr = const_vol;
        }
        else
        {
            if(div_ctr > 0) 
            {
                --div_ctr;
            }
            else
            {
                div_ctr = const_vol;
                if(decay_lvl_ctr > 0 || lectr_halt) --decay_lvl_ctr;
            }
        }
    }

    uint8_t vol()
    {
        return (use_const_vol ? const_vol : decay_lvl_ctr);
    }
};


}

#endif //ENVEL_H_NOS
