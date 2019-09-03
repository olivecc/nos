#ifndef  SHARED_BUS_H_NOS
#define  SHARED_BUS_H_NOS

#include <cstdint>
#include <cstddef>
#include <vector>

using std::vector;

namespace NES
{


enum : unsigned int
{
    width_px       = 256,
    height_px      = 240,
    scanln_width   = 341,
    scanln_height  = 262,
    hblank_end     = 320,
    pixel_quantity = width_px * height_px,
    ppu_ticks_per_cpu = 3,
    // Note: 3 PPU cycles per CPU cycle
    max_samples_per_frame = ((scanln_width * scanln_height) / 3) + 1
};

namespace IRQ_Src
{
    enum : unsigned int
    {
        APU_DMC   = 1U << 0,
        APU_FRAME = 1U << 1
    };
}

class Shared_Bus
{
  private:
    uint64_t frame_count = 0;

  public:
    template<class T, size_t N>
    class Double_Buffer
    {
      private:
        size_t index = 0;
        bool toggle = false;
        T fst[N] = { 0 };
        T snd[N] = { 0 };
        T        (&back())[N] { return (toggle ? fst : snd); }

      public:
        const T (&front())[N] { return (toggle ? snd : fst); }
        void push(T val) { back()[index++] = val; }
        void swap() { toggle = !toggle; index = 0; }
        Double_Buffer() {}
    };


    Double_Buffer<uint8_t, pixel_quantity>      framebuf;
    Double_Buffer<float, max_samples_per_frame> audiobuf;

    uint8_t ciram[0x800] = {0};

    uint16_t line_irq_low = 0;
    bool     line_nmi_low = false;

    bool is_apu_enabled = false;

    uint64_t cycle_count = 0;

    void push_frame()
    {
        ++frame_count;

        framebuf.swap();
        audiobuf.swap();
    }

    uint64_t get_frame_count() { return frame_count; }

    Shared_Bus() {}
};


}

#endif //SHARED_BUS_H_NOS
