#include <array>
#include <cstdint>      // uint8_t
#include <exception>    // runtime_error
#include <memory>       // unique_ptr, make_unique
#include <vector>
#include <cstring>      // memcpy

#include "cart.h"
#include "mapper.h"
#include "mapper00.h"

using std::array;
using std::runtime_error;
using std::make_unique;
using std::unique_ptr;
using std::vector;
            
static_assert(sizeof(uint8_t) == 1);

namespace
{


using Factory = unique_ptr<Mapper>(*)(const Header&);

template<typename M>
unique_ptr<Mapper> make_mapper(const Header& header)
{
    return make_unique<M>(header);
}

Factory get_mapper(uint8_t mapper_id)
{
    switch(mapper_id)
    {
        case(0x00): return make_mapper<Mapper00>;

        default:    throw runtime_error("Mapper not implemented");
    }
}


}

unique_ptr<NES::Cartridge> load_ines(const vector<uint8_t>& input)
{
    static constexpr unsigned int header_size = 0x10;
    
    auto fail = [](){ throw runtime_error("Invalid iNES file"); };

    if(input.size() < 0x10)
        fail();

                                            // ASCII character
    bool is_ines = ((input[0] == 0x4E) &&   // 'N'
                    (input[1] == 0x45) &&   // 'E'
                    (input[2] == 0x53) &&   // 'S'
                    (input[3] == 0x1A));    // SUB

    uint8_t prg_rom_size = input[4];
    uint8_t chr_rom_size = input[5];

    bool mirror_vertical  = (input[6] & (1U << 0));
    bool contains_nonvol  = (input[6] & (1U << 1));
    bool contains_trainer = (input[6] & (1U << 2));
    bool mirror_alt_mode  = (input[6] & (1U << 3));
    uint8_t mapper_id_lo  = (input[6] >> 4);
    uint8_t mapper_id_hi  = (input[7] >> 4);
    
    
    uint8_t mapper_id = ((mapper_id_hi << 4) | 
                         (mapper_id_lo << 0));
    
    size_t ines_size  = (0x10 + 
                         (prg_rom_size * prg_block_size) + 
                         (chr_rom_size * chr_block_size));

    
    bool is_valid_ines = (is_ines &&
                          !contains_trainer &&
                          (input.size() == ines_size));

    if(!is_valid_ines)
        fail();
    
    
    vector<array<uint8_t, prg_block_size>> prg_rom(prg_rom_size);
    vector<array<uint8_t, chr_block_size>> chr_rom(chr_rom_size);

    const uint8_t* prg_rom_src = input.data() + header_size;
    const uint8_t* chr_rom_src = prg_rom_src + (prg_rom_size * prg_block_size);

    auto populate_rom = [](auto& dst, const uint8_t* src, uint8_t block_num, 
            unsigned int block_size)
    {
        for(uint8_t i = 0; i < block_num; ++i)
        {
            memcpy(dst[i].data(), src + (i * block_size), block_size);
        }
    };

    populate_rom(prg_rom, prg_rom_src, prg_rom_size, prg_block_size);
    populate_rom(chr_rom, chr_rom_src, chr_rom_size, chr_block_size);

    bool has_chr_rom = (chr_rom_size > 0);
    if(!has_chr_rom)
        chr_rom.resize(1);

    Header header = 
    { 
        mirror_vertical, 
        contains_nonvol, 
        mirror_alt_mode, 
        has_chr_rom, 
        prg_rom, 
        chr_rom 
    };

    Factory make_mapper = get_mapper(mapper_id);
    return make_mapper(header);
}
