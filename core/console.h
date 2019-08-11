#ifndef   CONSOLE_H_NOS
#define   CONSOLE_H_NOS

#include "shared_bus.h"
#include "controller.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
//include mapper

#include <cstdint>      // uint8_t, uint32_t
#include <memory>       // unique_ptr
#include <vector>
#include <cstddef>

using std::unique_ptr;
using std::vector;
using std::make_unique;

namespace NES
{


static constexpr double clock_speed_hz = (1000 * 1000) * (236.25 / 11);
static constexpr double cpu_clock_speed_hz = clock_speed_hz / 12;

class Console
{
  public:
    Shared_Bus shared_bus;
    unique_ptr<Controller> port_one;
    unique_ptr<Controller> port_two;
    //unique_ptr<Mapper> mapper;
    PPU ppu;
    APU apu;
    CPU cpu;

  public:
    const uint8_t (&get_framebuf())[pixel_quantity]
    {
        return shared_bus.framebuf.front();
    }

    const float (&get_audiobuf())[max_samples_per_frame]
    {
        return shared_bus.audiobuf.front();
    }

    uint64_t get_frame_count() { return shared_bus.get_frame_count(); }

    void set_port_one(Controller::Button btn, bool is_pressed)
    { port_one->set_state(btn, is_pressed); }

    void set_port_two(Controller::Button btn, bool is_pressed)
    { port_two->set_state(btn, is_pressed); }

    void exec() { cpu.execute_instruction(); }

    Console(vector<uint8_t> prg, vector<uint8_t> chr) 
        : shared_bus(), 
          port_one(make_unique<Controller>()),
          port_two(make_unique<Controller>()), 
          ppu(&shared_bus, chr),
          apu(&shared_bus),
          cpu(&shared_bus, &ppu, &apu, port_one.get(), port_two.get(), prg) {}
};


}
#endif // CONSOLE_H_NOS
