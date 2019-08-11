#ifndef  LECTR_H_NOS
#define  LECTR_H_NOS

namespace NES
{


class Length_Counter
{
  private:
    static constexpr uint8_t clock_table[0x20] = 
    {
        0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06, 
        0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E, 
        0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16, 
        0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E 
    };

    bool enabled;
    uint8_t clock;

  public:
    bool halt;

    void set_enabled(bool val)
    {
        enabled = val;
        if(!enabled) clock = 0;
    }

    void write_d(uint8_t data)
    {
        if(enabled)
        {
            uint8_t index = (data >> 3);
            clock = clock_table[index];
        }
    }

    bool is_active() { return (clock > 0); }

    void tick_frame_half()
    {
        if(clock > 0 && !halt)
            --clock;
    }
};


}

#endif //LECTR_H_NOS
