#ifndef  MAPPER00_H_NOS
#define  MAPPER00_H_NOS

#include <cstdint>

#include "mapper.h"
#include "mapper_impl.h"
#include "shared_bus.h"

// NROM
class Mapper00 : public Mapper_Impl<14, 13>
{
  private:
    uint8_t prg_ram[0x2000];

  public:
    Mapper00(const Header& header) : Mapper_Impl(header) {}

    uint8_t cpu_read(Shared_Bus&, uint16_t addr) override
    {
        uint8_t val = 0;

        if     (addr >= 0xC000) val = access_prg(1, addr % 0x4000);
        else if(addr >= 0x8000) val = access_prg(0, addr % 0x4000);
        else if(addr >= 0x6000) val =       prg_ram[addr % 0x2000];

        return val;
    }

    void cpu_write(Shared_Bus&, uint16_t addr, uint8_t data) override
    {
        if(addr >= 0x6000 && addr < 0x8000)
            prg_ram[addr % 0x2000] = data;
    }

    uint8_t& pt_access(Shared_Bus& shared_bus, uint16_t addr) override
    {
        return access_chr(0, addr);
    }
};

#endif //MAPPER00_H_NOS
