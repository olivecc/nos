#ifndef  LICTR_H_NOS
#define  LICTR_H_NOS

#include <cstdint>

namespace NES
{


class Linear_Counter
{
  private:
    bool should_reload;
    uint8_t clock_reload : 7;
    uint8_t clock : 7;
    bool& lectr_halt;

  public:
    Linear_Counter(bool& lectr_halt) : lectr_halt(lectr_halt) {}

    bool is_active() { return (clock > 0); }

    void tick_frame_quarter()
    {
        if(should_reload) 
            clock = clock_reload;
        else if(clock > 0) 
            --clock;

        if(!lectr_halt) 
            should_reload = false;
    }

    void write_a(uint8_t data)
    {
        lectr_halt = data & (1U << 7);
        clock_reload = data & (0xFFU >> 1);
    }

    void write_d(uint8_t data)
    {
        should_reload = true;
    }
};


}

#endif //LICTR_H_NOS
