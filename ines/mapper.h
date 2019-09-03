#ifndef  MAPPER_H_NOS
#define  MAPPER_H_NOS

#include <cstdint>
#include <tuple>

#include "cart.h"
#include "shared_bus.h"
#include "header.h"

class Mapper : public NES::Cartridge
{
  private:
    Header header;

    static uint32_t get_bank_num(uint8_t block_num, unsigned int bank_size_exp,
        unsigned int block_size_exp)
    {
        return (((uint32_t)block_num << block_size_exp) >> bank_size_exp);
    }

    template<typename T>
    static uint8_t& access_rom(T& rom, uint32_t bank, uint32_t bank_num,
            unsigned int bank_size_exp, unsigned int block_size_exp, 
            uint32_t sub_addr)
    {
        bank %= bank_num;
        uint32_t full_addr = ((bank << bank_size_exp) | sub_addr);
        
        uint8_t  block_index = full_addr >> block_size_exp;
        uint16_t block_addr  = full_addr & ~(0xFFFFU << block_size_exp);

        return rom[block_index][block_addr];
    }

  protected:
    using Shared_Bus = NES::Shared_Bus; 

    virtual unsigned int get_prg_bank_size_exp() = 0;
    virtual unsigned int get_chr_bank_size_exp() = 0;

    uint32_t get_prg_bank_num() 
    {
        return get_bank_num(header.prg.size(), get_prg_bank_size_exp(), 
            prg_block_size_exp);
    }
    
    uint32_t get_chr_bank_num() 
    {
        return get_bank_num(header.chr.size(), get_chr_bank_size_exp(), 
            chr_block_size_exp);
    }

    uint8_t& access_prg(uint32_t bank, uint32_t sub_addr)
    {
        return access_rom(header.prg, bank, get_prg_bank_num(),
            get_prg_bank_size_exp(), prg_block_size_exp, sub_addr);
    }
    
    uint8_t& access_chr(uint32_t bank, uint32_t sub_addr)
    {
        return access_rom(header.chr, bank, get_chr_bank_num(), 
            get_chr_bank_size_exp(), chr_block_size_exp, sub_addr);
    }

    virtual uint8_t& pt_access(Shared_Bus& shared_bus, uint16_t addr) = 0;

    virtual uint8_t& nt_access(Shared_Bus& shared_bus, uint16_t addr)
    {
        uint16_t nt_mask = 1U << (header.mirror_vertical ? 10 : 11);
        uint16_t ci_bit = ((addr & nt_mask) ? 1U : 0U) << 10;
        uint16_t ci_subaddr = (addr & ~(0xFFFFU << 10));
        return shared_bus.ciram[ci_bit | ci_subaddr];
    }

    uint8_t& ppu_access(Shared_Bus& shared_bus, uint16_t addr)
    {
        uint16_t sub_addr = (addr & ~(0xFFFFU << 13));
        return (((addr & (1U << 13)) == 0)
            ? pt_access(shared_bus, addr & ~(0xFFFFU << 13))
            : nt_access(shared_bus, addr & ~(0xFFFFU << 12)));
    }

  public:
    Mapper(const Header& header) : header(header) {}

    uint8_t ppu_read(Shared_Bus& shared_bus, uint16_t addr) override
    {
        return ppu_access(shared_bus, addr);
    }

    void ppu_write(Shared_Bus& shared_bus, uint16_t addr, uint8_t data) override
    {
        uint8_t& dst = ppu_access(shared_bus, addr);
        dst = data;
    }
};

#endif //MAPPER_H_NOS
