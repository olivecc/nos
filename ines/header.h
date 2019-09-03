#ifndef  HEADER_H_NOS
#define  HEADER_H_NOS

#include <array>
#include <vector>

enum : unsigned int
{
    prg_block_size_exp = 14,
    chr_block_size_exp = 13,
    prg_block_size = (1U << prg_block_size_exp),
    chr_block_size = (1U << chr_block_size_exp)
};

struct Header
{
    bool mirror_vertical;
    bool contains_nonvol;
    bool mirror_alt_mode;
    bool has_chr_rom;

    std::vector<std::array<uint8_t, prg_block_size>> prg;
    std::vector<std::array<uint8_t, chr_block_size>> chr;
};

#endif //HEADER_H_NOS
