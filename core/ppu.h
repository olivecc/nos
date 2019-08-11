#ifndef   PPU_H_NOS
#define   PPU_H_NOS

#include "shared_bus.h"
//include mapper

#include <cstdint>      // uint8_t, uint16_t, uint64_t
#include <cstring>      // memcpy
#include <vector>
#include <utility>      //pair

using std::memcpy;
using std::vector;
using std::pair;

namespace NES
{


class PPU
{
  private:
    Shared_Bus* shared_bus;

    uint8_t ntram[0x800] = {0};
    uint8_t palette_bg[0xC] = {0};
    uint8_t palette_sp[0xC] = {0};
    uint8_t palette_misc[0x4] = {0};
    uint8_t oam[0x100] = {0};
    uint8_t oam_aux[0x20] = {0};
    uint8_t pattern_table[0x2000] = {0};

    uint16_t vram_addr = 0;
    uint16_t vram_addr_tmp = 0;
    uint8_t  scroll_x_fine = 0;
    bool     write_toggle;
    uint16_t vram_addr_bus = 0;

    uint16_t tile_sliver_addr = 0;
    uint16_t bg_tile_shift_lo = 0;
    uint16_t bg_tile_shift_hi = 0;
    uint8_t palette_shift_lo = 0;
    uint8_t palette_shift_hi = 0;
    uint8_t bg_palette_latch = 0;
    uint8_t next_bg_palette = 0;
    uint8_t next_bg_tile_lo = 0;
    uint8_t next_bg_tile_hi = 0;

    uint8_t sp_tile_shift_lo[8] = {0};
    uint8_t sp_tile_shift_hi[8] = {0};
    uint8_t sp_attr[8] = {0};
    uint8_t sp_xpos[8] = {0};
    uint8_t sp_ypos = 0;

    uint8_t oam_aux_addr = 0;
    bool oam_aux_full = false;
    uint8_t sprite_bytes_copied = 0;
    bool sprite_in_range = false;
    bool oam_scanned = false;
    uint8_t sprite_count = 0;
    uint8_t overflow_cycle_count = 0;

    bool ctrl_write_ver_hor;
    bool ctrl_sp_pattern_table;
    bool ctrl_bg_pattern_table;
    bool ctrl_sprites_large;
    bool ctrl_master_slave;
    bool ctrl_nmi_output;
    bool mask_grayscale;
    bool mask_show_bg_left;
    bool mask_show_sp_left;
    bool mask_show_bg;
    bool mask_show_sp;
    bool mask_emph_r;
    bool mask_emph_g;
    bool mask_emph_b;
    bool stat_sp_overflow;
    bool stat_sp_zero_hit;
    bool stat_nmi_occurred;
    bool new_nmi_occurred;
    uint8_t oam_addr;
    uint8_t vram_read_buf;
    uint8_t oam_buf = 0;
    
    uint64_t cycle_count;
    uint8_t reg_latch = 0;
    bool even_odd_frame = false;
    bool nt_mirror_vert_hori = false;

    uint8_t read_oam(uint8_t addr)               { return oam[addr]; }
    void   write_oam(uint8_t addr, uint8_t data) { oam[addr] = data; }

    // Precondition: addr < 0x20
    uint8_t read_oam_aux(uint8_t addr)               { return oam_aux[addr]; }
    void   write_oam_aux(uint8_t addr, uint8_t data) { oam_aux[addr] = data; }

    static uint16_t splice_bits(uint16_t src, uint8_t src_start, 
                                   uint16_t dst, uint8_t dst_start, 
                                   uint8_t num)
    {
        const uint16_t mask = ~(0xFFFFU << num);

        uint16_t bits = ((src >> src_start) & mask);
        dst &= ~(mask << dst_start);
        dst |=  (bits << dst_start);

        return dst;
    }

    void increment_cycle_count()
    { 
        ++cycle_count; 
        cycle_count %= (scanln_width * scanln_height); 
    }

    uint8_t sprite_height() { return (ctrl_sprites_large ? 16 : 8); }

    void set_vram_addr_bus(uint16_t addr)
    {
        vram_addr_bus = (addr % 0x4000);
    }

    bool is_rendering_enabled()
    {
        return (mask_show_bg || mask_show_sp);
    }

    bool is_render_scanln()
    {
        return ((scanln() < height_px) || (scanln() == scanln_height - 1));
    }

    bool is_rendering()
    {
        return (is_render_scanln() && is_rendering_enabled());
    }

    uint8_t& ntram_access(uint16_t addr)
    {
        // The presence/absence of this bit in addr indicates that addr refers
        // to the nametable starting at address 0x400/0x0 in VRAM, respectively
        uint16_t nt_mask = 1U << (nt_mirror_vert_hori ? 10 : 11);
        uint16_t nt_access_addr = (addr & nt_mask) ? 0x400 : 0x0;
        return ntram[nt_access_addr + (addr % 0x400)];
    }

    // Palette RAM
    uint8_t& pram_access(uint8_t addr)
    {
        uint8_t pal_index = addr % 0x10;
        if(pal_index % 0x4 != 0)
        { 
            uint8_t (&pal_table)[0xC] = (((addr & (1U << 4)) == 0) 
                ? palette_bg
                : palette_sp);
            return pal_table[pal_index - ((pal_index / 0x4) + 0x1)];
        }
        else
        {
            return palette_misc[pal_index / 0x4];
        }
    }

    uint8_t& cart_access(uint16_t addr)
    {
        return (((addr & (1U << 13)) == 0)
            ? pattern_table[addr]
            : ntram_access(addr));
    }

    uint8_t& vram_access(uint16_t addr)
    {
        addr %= 0x4000;

        set_vram_addr_bus(addr);

        return ((addr < 0x3F00) 
            ? cart_access(addr)
            : pram_access(addr));
    }

    uint8_t read_vram(uint16_t addr)
    {
        return vram_access(addr);
    }

    void write_vram(uint16_t addr, uint8_t data)
    {
        vram_access(addr) = data;
    }

    uint16_t nt_addr()
    {
        return (0x2000U | (vram_addr % 0x1000));
    }

    uint16_t attr_addr()
    {
        return (0x2000U | 
                ((~(0xFFFFU << 2) << 10) & vram_addr) | 
                ( ~(0xFFFFU << 4) << 6) |
                ((~(0xFFFFU << 3) & (vram_addr >> 7)) << 3) |
                ((~(0xFFFFU << 3) & (vram_addr >> 2)) << 0));
    }

    void clear_oam_aux()
    {
        if(dot() == 1)  oam_aux_addr = 0; 
        
        if(dot() % 2) 
        {
            oam_buf = 0xFF;
        }
        else          
        {
            write_oam_aux(oam_aux_addr, oam_buf);
            
            ++oam_aux_addr;
            oam_aux_addr %= 0x20;
        }
    }

    void perform_sprite_evaluation()
    {
        if(dot() == 65)
        {
            sprite_count = 0;
            sprite_in_range = false;
            oam_aux_full = false;
            oam_scanned = false;
            overflow_cycle_count = 0;
        }

        if(dot() % 2)
        {
            oam_buf = read_oam(oam_addr);
        }
        else
        {
            bool read_signal = (oam_scanned || oam_aux_full);
            
            if(read_signal) oam_buf = read_oam_aux(oam_aux_addr);
            else            write_oam_aux(oam_aux_addr, oam_buf);

            if(oam_scanned)
            {
                oam_addr += 0x4;
            }
            else
            {
                if((scanln() >= oam_buf) && 
                   (scanln() < (oam_buf + sprite_height())))
                {
                    sprite_in_range = true;
                }

                if(sprite_in_range)
                {
                    ++oam_addr;

                    if(oam_aux_full)
                    {
                        stat_sp_overflow = true;
                        ++overflow_cycle_count;
                        if(overflow_cycle_count == 4) oam_scanned = true;
                    }
                    else
                    {
                        ++oam_aux_addr;
                        oam_aux_addr %= 0x20;
                        if(oam_aux_addr % 0x4 == 0)
                        {
                            ++sprite_count;
                            sprite_in_range = false;
                            if(oam_aux_addr   == 0) oam_aux_full = true; 
                            if(oam_addr / 0x4 == 0) oam_scanned = true;
                        }
                    }
                }
                else
                {
                    if(oam_aux_full)
                    {
                        if(oam_addr % 4 != 3) ++oam_addr;
                        else                  oam_addr &= (0xFFFFU << 2);
                    }

                    oam_addr += 4;
                    if(oam_addr / 4 == 0) oam_scanned = true;
                }
            }

        }
    }

    
    void increment_scroll_x_coarse()
    {
        constexpr uint16_t mask_coarse = ~(0xFFFFU << 5);
        if((vram_addr & mask_coarse) != mask_coarse)
        {
            vram_addr += (1U << 0);
        }
        else
        {
            // Wrap to next nametable
            vram_addr &= ~mask_coarse;
            vram_addr ^= (1U << 10);
        }
    }

    void increment_scroll_y()
    {
        constexpr uint16_t mask_fine = ((0xFFFFU << 12) & ~(1U << 15));
        if((vram_addr & mask_fine) != mask_fine)
        {
            vram_addr += (1U << 12);
        }
        else
        {
            vram_addr &= ~mask_fine;
            // Add fine Y increment carry to coarse Y
            constexpr uint16_t mask_coarse = ((~(0xFFFFU << 5)) << 5);
            constexpr uint16_t coarse_max = 29;
            uint8_t coarse = ((vram_addr & mask_coarse) >> 5);
            if(coarse == coarse_max)
            {
                // Wrap to next nametable
                coarse = 0;
                vram_addr ^= (1U << 11);
            }
            else
            {
                ++coarse;
            }
            vram_addr &= ~mask_coarse;
            vram_addr |= ((coarse << 5) & mask_coarse);
        }
    }

    void reload_scroll_x_coarse()
    {
        constexpr uint16_t mask = ((1U << 10) | ~(0xFFFFU << 5));
        vram_addr &= ~mask;
        vram_addr |= (vram_addr_tmp & mask);
    } 

    void reload_scroll_y()
    {
        constexpr uint16_t mask = ((0xFFFFU << 12) | (1U << 11) |
                                   (~(0xFFFFU << 5) << 5));
        vram_addr &= ~mask;
        vram_addr |= (vram_addr_tmp & mask);
    }

    void increment_vram_addr()
    {
        if(is_rendering())
        {
            increment_scroll_x_coarse();
            increment_scroll_y();
        }
        else
        {
            vram_addr += (ctrl_write_ver_hor ? 32 : 1);
            vram_addr &= ~(1U << 15);

            set_vram_addr_bus(vram_addr);
        }
    }

    void shift_bg_registers()
    {
        bg_tile_shift_lo <<= 1;
        bg_tile_shift_hi <<= 1;

        palette_shift_lo <<= 1;
        palette_shift_hi <<= 1;
        palette_shift_lo |= ((bg_palette_latch >> 0) & 1U);
        palette_shift_hi |= ((bg_palette_latch >> 1) & 1U);

        if(dot() % 0x8 == 0)
        {
            bg_tile_shift_lo |= next_bg_tile_lo;
            bg_tile_shift_hi |= next_bg_tile_hi;
            bg_palette_latch  = next_bg_palette;
        }
    }

    void shift_sp_registers()
    {
        for(unsigned int i = 0; i < 0x8; ++i)
        {
            if(sp_xpos[i] != 0)
            {
                --(sp_xpos[i]);
            }
            else
            {
                bool flip_hori = (sp_attr[i] & (1U << 6));
                auto shift = [flip_hori](uint8_t& reg) -> void
                { flip_hori ? (reg >>= 1) : (reg <<= 1); };

                shift(sp_tile_shift_lo[i]);
                shift(sp_tile_shift_hi[i]);
            }
        }
    }

    uint8_t hblank_dot()
    {
        return (dot() - (width_px + 1));
    }

    void fetch_sp_tile_data()
    {
        // Relies on circuitry of background tile fetch

        if(dot() == width_px + 1) oam_aux_addr = 0;
        
        uint8_t sp_index = hblank_dot() / 8;
        
        oam_buf = read_oam_aux(oam_aux_addr);

        switch(hblank_dot() % 8)
        {
            case(0x0):
            {
                sp_ypos = oam_buf;
                ++oam_aux_addr;
                break;
            }
            case(0x1):
            {
                uint8_t tile_index = oam_buf;
                uint8_t sprite_y = scanln() - sp_ypos;
                uint8_t sliver_offset = (((sprite_y & ~(1U << 3)) << 0) |
                                         ((sprite_y &  (1U << 3)) << 1));
                tile_sliver_addr = (ctrl_sprites_large
                    ? (((tile_index &  1U) << 12) |
                       ((tile_index & ~1U) <<  4) |
                       sliver_offset)
                    : (((ctrl_sp_pattern_table ? 1U : 0U) << 12) |
                       (tile_index << 4) | 
                       sliver_offset));
                ++oam_aux_addr;
                break;
            }
            // Effect of ctrl_sprites_large changing between these cycles?
            case(0x2):
            {
                sp_attr[sp_index] = oam_buf;
                
                bool is_vertically_mirrored = (oam_buf & (1U << 7));
                if(is_vertically_mirrored)
                {
                    uint8_t mask = (~(0xFFFFU << 2) | 
                                    ((ctrl_sprites_large ? 1U : 0U) << 3));
                    tile_sliver_addr ^= mask;
                }
                ++oam_aux_addr;
                break;
            }
            case(0x7):
            {
                using byte_pair = pair<uint8_t, uint8_t>;
                auto [new_lo, new_hi] = ((sp_index < sprite_count)
                        ? byte_pair{ next_bg_tile_lo, next_bg_tile_hi }
                        : byte_pair{ 0, 0 });
                sp_tile_shift_lo[sp_index] = new_lo;
                sp_tile_shift_hi[sp_index] = new_hi;
                ++oam_aux_addr;
            }
            default:
            {
                sp_xpos[sp_index] = oam_buf;
                break;
            }
        }
    }

    void fetch_bg_tile_data()
    {
        // Strictly, background tile operations should take two cycles each to
        // execute; here, however, they are simulated as taking one each
        
        switch(hblank_dot() % 8)
        {
            case(0x0):
            {
                uint8_t tile_index = read_vram(nt_addr());
                tile_sliver_addr = (((ctrl_bg_pattern_table ? 1U : 0U) << 12) |
                                    (tile_index << 4) |
                                    (vram_addr >> 12));
                break;
            }

            case(0x2):
            {
                unsigned int shamt = (((vram_addr >> 6) & 1U) << 1 |
                                      ((vram_addr >> 1) & 1U)) * 2;
                uint8_t mask = 0xFU >> 2;
                next_bg_palette = (read_vram(attr_addr()) >> shamt) & mask;
                break;
            }

            case(0x4):
            {
                next_bg_tile_lo = read_vram(tile_sliver_addr);
                break;
            }

            case(0x6):
            {
                next_bg_tile_hi = read_vram(tile_sliver_addr + 8);
                break;
            }
        }
    }

    uint8_t get_pixel_color()
    {
        bool bg_masked = (!mask_show_bg_left && (dot() <= 8));
        bool sp_masked = (!mask_show_sp_left && (dot() <= 8));

        unsigned int shamt = scroll_x_fine;

        auto lshift = [shamt](uint16_t lo, uint16_t hi) -> uint8_t
        {
            return (((((lo << shamt) >> 7) & 1U) << 0) |
                    ((((hi << shamt) >> 7) & 1U) << 1));
        };
        auto rshift = [shamt](uint16_t lo, uint16_t hi) -> uint8_t
        {
            return ((((lo >> shamt) & 1U) << 0) |
                    (((hi >> shamt) & 1U) << 1));
        };

        uint8_t bg_color = ((mask_show_bg && !bg_masked)
            ? lshift(bg_tile_shift_lo >> 8, 
                     bg_tile_shift_hi >> 8)
            : 0);

        if(mask_show_sp && !sp_masked)
        {
            for(unsigned int i = 0; i < 8; ++i)
            {
                if(sp_xpos[i] == 0)
                {
                    bool flip_hori = (sp_attr[i] & (1U << 6));
                    uint8_t sp_lo = sp_tile_shift_lo[i];
                    uint8_t sp_hi = sp_tile_shift_hi[i];
                    uint8_t sp_color = (flip_hori 
                                        ? rshift(sp_lo, sp_hi)
                                        : lshift(sp_lo, sp_hi));
                    if(sp_color != 0)
                    {
                        if((i == 0) && (bg_color != 0) &&
                           (dot() != width_px))
                        {
                            stat_sp_zero_hit = true;
                        }

                        bool has_front_priority = !(sp_attr[i] & (1U << 5));
                        if((bg_color == 0) || (has_front_priority))
                        {
                            uint8_t sp_palette = (sp_attr[i] & (0xFU >> 2));
                            return ((1U << 4) | (sp_palette << 2) | sp_color);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }
        
        uint8_t bg_palette = lshift(palette_shift_lo, 
                                          palette_shift_hi);
        return ((bg_palette << 2) | bg_color);
    }

    bool is_warming_up()
    {
        unsigned int cpu_warmup_cycles = 29658;
        return ((shared_bus->get_frame_count() == 0) && 
                (cycle_count < (cpu_warmup_cycles * 3)));
    }

  public:
    PPU(Shared_Bus* shared_bus, vector<uint8_t> chr) : shared_bus(shared_bus)
    {
        memcpy(pattern_table, chr.data(), 0x2000 * sizeof(uint8_t));
        reset_state(true);
    }

    // Update APU status at start of every scanline?
    void execute_cycle()
    {
        stat_nmi_occurred = new_nmi_occurred;

        if(is_render_scanln())
        {
            bool is_visible_scanln = (scanln() < height_px);

            if(dot() == 0) 
            { 
                if(!is_visible_scanln)
                {
                    stat_sp_overflow = false;
                    stat_sp_zero_hit = false;
                    new_nmi_occurred = false;
                    even_odd_frame = !even_odd_frame;

                    if(is_rendering_enabled() && (oam_addr > 0x8))
                    {
                        uint8_t mask = (0xFFU << 3);
                        for(unsigned int i = 0; i < 8; ++i)
                        {
                            uint8_t data = read_oam((oam_addr & mask) + i);
                            write_oam(i, data);
                        }
                    }
                }
            }
            else if(dot() <= width_px)
            {
                if(is_rendering_enabled())
                {
                    fetch_bg_tile_data();

                    if(dot() % 8 == 0)        increment_scroll_x_coarse();
                    if(dot() == width_px) increment_scroll_y();

                    if(dot() <= 64)            clear_oam_aux();
                    else if(is_visible_scanln) perform_sprite_evaluation();
                }

                if(is_visible_scanln)
                {
                    uint8_t color_index = (((vram_addr % 0x4000 < 0x3F00) || 
                                            is_rendering_enabled())
                        ? get_pixel_color()
                        : vram_addr);
                    uint8_t px_color = pram_access(color_index) & (0xFFU >> 2);
                    shared_bus->framebuf.push(px_color);
                }
                    
                shift_bg_registers();
                shift_sp_registers();
            }
            else if(dot() <= hblank_end)
            {
                if(is_rendering_enabled())
                {
                    if(dot() == width_px + 1) reload_scroll_x_coarse();
                    
                    oam_addr = 0;

                    fetch_bg_tile_data();
                    fetch_sp_tile_data();
                    
                    if((dot() >= 280) && (dot() <= 304))
                    {
                        if(!is_visible_scanln) reload_scroll_y();
                    }
                }
            }
            else if(dot() < (scanln_width - 4))
            {
                if(is_rendering_enabled())
                {
                    fetch_bg_tile_data();

                    if(dot() == hblank_end + 1) oam_buf = read_oam_aux(0);
                    if(dot() % 8 == 0) increment_scroll_x_coarse();
                }

                shift_bg_registers();
            }
            else
            {
                if(is_rendering_enabled()) 
                {
                    if(dot() == scanln_width - 1)
                    {
                        if(!is_visible_scanln && !even_odd_frame) 
                        {
                            increment_cycle_count();
                        }
                    }

                    if(dot() % 2) read_vram(nt_addr());
                }
            }
        }
        else if(scanln() == height_px)
        {
            if(dot() == 0) 
            {
                set_vram_addr_bus(vram_addr);
                
                shared_bus->push_frame();
            }
            else { /* Idle */ }
        }
        else if(scanln() == height_px + 1)
        {
            if(dot() == 0) new_nmi_occurred = true;
            else           { /* Idle */ }
        }

        increment_cycle_count();
        shared_bus->cycle_count += 4;
        shared_bus->line_nmi_low = (ctrl_nmi_output && stat_nmi_occurred);
    }

    
    uint8_t read_reg(uint8_t reg_index)
    {
        uint8_t mask = 0x00;
        uint8_t value = 0x00;

        switch(reg_index)
        {
            case(0x2):
            {
                value = (((stat_sp_overflow  ? 1U : 0U) << 5) | 
                         ((stat_sp_zero_hit  ? 1U : 0U) << 6) |
                         ((stat_nmi_occurred ? 1U : 0U) << 7));
                new_nmi_occurred = false;
                write_toggle = 0;
                mask = (0xFFU << 5);
                break;
            }
            case(0x4):
            {
                // Emulator option to disable read?
                value = (is_rendering() ? oam_buf : read_oam(oam_addr));
                mask = 0xFF;
                break;
            }
            case(0x7):
            {
                using byte_pair = pair<uint8_t, uint8_t>;
                auto [new_value, new_mask] = ((vram_addr_bus < 0x3F00) 
                    ? byte_pair{ vram_read_buf,         0xFF }
                    : byte_pair{ pram_access(vram_addr_bus), ~(0xFFU << 6) });
                value = new_value;
                mask = new_mask;
                vram_read_buf = cart_access(vram_addr_bus);
                increment_vram_addr();
                break;
            }
        }

        reg_latch &= ~mask;
        reg_latch |= value;
        return reg_latch;
    }
    
    void write_reg(uint8_t reg_index, uint8_t data)
    {
        reg_latch = data;

        switch(reg_index)
        {
            case(0x0):
            {
                if(is_warming_up()) break;
                vram_addr_tmp = splice_bits(data, 0, vram_addr_tmp, 10, 2);
                ctrl_write_ver_hor      = ((data >> 2) & 1U);
                ctrl_sp_pattern_table   = ((data >> 3) & 1U);
                ctrl_bg_pattern_table   = ((data >> 4) & 1U);
                ctrl_sprites_large      = ((data >> 5) & 1U);
                ctrl_master_slave       = ((data >> 6) & 1U);
                ctrl_nmi_output         = ((data >> 7) & 1U);
                // Race condition on bit 0 on dot 257?
                break;
            }
            case(0x1):
            {
                if(is_warming_up()) break;;
                mask_grayscale          = ((data >> 0) & 1U);
                mask_show_bg_left       = ((data >> 1) & 1U);
                mask_show_sp_left       = ((data >> 2) & 1U);
                mask_show_bg            = ((data >> 3) & 1U);
                mask_show_sp            = ((data >> 4) & 1U);
                mask_emph_r             = ((data >> 5) & 1U);
                mask_emph_g             = ((data >> 6) & 1U);
                mask_emph_b             = ((data >> 7) & 1U);
                //TODO implement color emphasis/grayscale
                break;
            }
            case(0x3):
            {
                // Behaviour during rendering (sprite evaluation)?
                oam_addr = data;
                // Corrupt OAM?
                break;
            }
            case(0x4):
            {
                if(is_rendering())
                {
                    oam_addr += 0x4;
                }
                else
                {
                    // Avoids storing unimplemented bits of sprite attributes
                    if((oam_addr % 0x4) == 0x2) 
                        data = splice_bits(0, 0, data, 2, 3);

                    write_oam(oam_addr, data);
                    ++oam_addr;
                }
                break;
            }
            case(0x5):
            {
                if(is_warming_up()) break;
                if(!write_toggle)
                {
                    scroll_x_fine = splice_bits(data, 0, scroll_x_fine, 0, 3);
                    vram_addr_tmp = splice_bits(data, 3, vram_addr_tmp, 0, 5);
                }
                else
                {
                    vram_addr_tmp = splice_bits(data, 0, vram_addr_tmp, 12, 3);
                    vram_addr_tmp = splice_bits(data, 3, vram_addr_tmp, 5,  5);
                }
                write_toggle = !write_toggle;
                break;
            }
            case(0x6):
            {
                if(is_warming_up()) break;
                if(!write_toggle)
                {
                    vram_addr_tmp = splice_bits(data, 0, vram_addr_tmp, 8, 6);
                    vram_addr_tmp &= ~(1U << 14);
                }
                else
                {
                    vram_addr_tmp = splice_bits(data, 0, vram_addr_tmp, 0, 8);
                    vram_addr = vram_addr_tmp;
                    if(!is_rendering()) set_vram_addr_bus(vram_addr);
                }
                write_toggle = !write_toggle;
                break;
            }
            case(0x7):
            {
                // Corrupted write during rendering?
                write_vram(vram_addr_bus, data);
                increment_vram_addr();
                break;
            }
        }
    }
    
    uint16_t dot()    { return cycle_count % scanln_width; }
    uint16_t scanln() { return cycle_count / scanln_width; }

    void reset_state(bool is_power_cycle)
    {
        // Random values?
        bool random = true;

        ctrl_write_ver_hor = false;
        ctrl_sp_pattern_table = false;
        ctrl_bg_pattern_table = false;
        ctrl_sprites_large = false;
        ctrl_master_slave = false;
        ctrl_nmi_output = false;

        mask_grayscale = false;
        mask_show_bg_left = false;
        mask_show_sp_left = false;
        mask_show_bg = false;
        mask_show_sp = false;
        mask_emph_r = false;
        mask_emph_g = false;
        mask_emph_b = false;

        write_toggle = false;
        vram_addr_tmp = 0;
        scroll_x_fine = 0;

        vram_read_buf = 0;
        even_odd_frame = true;

        cycle_count = 0;
        //frame_count = 0;
        
        // OAM unspecified
        
        if(is_power_cycle)
        {
            stat_sp_overflow = random;
            stat_sp_zero_hit = false;
            new_nmi_occurred = random;
            stat_nmi_occurred = new_nmi_occurred;

            oam_addr = 0;
            set_vram_addr_bus(0);

            // Palette RAM, NTRAM, CHRAM unspecified
        }
    }
};


}
#endif // PPU_H_NOS
