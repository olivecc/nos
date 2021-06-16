#ifndef CPU_H_NOS
#define CPU_H_NOS

#include "shared_bus.h"
#include "cart.h"
#include "ppu.h"
#include "apu.h"

#include <cstdint>  // uint8_t, uint16_t
#include <cstring>  // memcpy
#include <utility>  // pair
#include <vector>

using std::pair;
using std::vector;

namespace NES
{


/* 
 * The 'Relative' addressing mode is omitted here and all instances replaced
 * with the 'Immediate' mode (Imm) instead, due to behaving identically
 * 
 * The ___S addressing modes listed below are used as 'safe' versions of the ___
 * addressing modes to avoid page-boundary cycle optimisations, which can have
 * adverse effects on memory if used to write to memory
 */

enum AddrMode : unsigned int
{
    Imp,  Acc,                                  // No operand
    Imm,  ZP,   ZPX,  ZPY,  InX,  InY,  InYS,   // One-byte operand
    Ab,   AbX,  AbXS, AbY,  AbYS, In            // Two-byte operand
};

enum Instr : unsigned int
{
    ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC, CLD,
    CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP, JSR, LDA,
    LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI, RTS, SBC, SEC,
    SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA, STP, SLO, RLA, SRE,
    RRA, SAX, LAX, DCP, ISC, ANC, ALR, ARR, XAA, AXS, AHX, TAS, LAS, SHX, SHY
};

// Six status flag registers are present in hardware, but they are often pushed
// to the stack as a byte, with two other bits included as 'pseudo-flags'.
// U is never used and always 1, while B is set to 1 or 0 representing
// whether the flags were pushed by an instruction (BRK/PHP) or by hardware
// interrupt (IRQ/NMI) respectively (also used as an emulation flag here).
namespace PS_Flags
{  
    enum : uint8_t
    {
        CARRY       = 1U << 0,      // C
        ZERO        = 1U << 1,      // Z
        IRQ_DISABLE = 1U << 2,      // I
        DECIMAL     = 1U << 3,      // D
        BREAK       = 1U << 4,      // B (always low)
        UNUSED      = 1U << 5,      // U (always low)
        OVERFLOW    = 1U << 6,      // V
        NEGATIVE    = 1U << 7       // N
    };
}


namespace IV_Addr
{
    enum : uint16_t
    {
        NMI =   0xFFFA,
        RESET = 0xFFFC,
        IRQ =   0xFFFE
    };
}


// Ricoh 2A03
class CPU
{
  private:

    Shared_Bus& shared_bus;
    Cartridge& cart;
    PPU& ppu;
    APU& apu;
    Controller& port_one;
    Controller& port_two;

    uint8_t ram[0x800] = { 0 };

    uint8_t A;              // Accumulator
    uint8_t X, Y;           // Index (general-purpose) registers
    uint8_t PS;             // Processor status (flags)
    uint8_t SP;             // Stack pointer
    uint16_t PC;            // Program counter

    uint16_t effective_operand;     // Derived from actual operand (if supplied)
    bool should_branch = false;

    bool ignore_irq_change = false;
    bool ignore_nmi_change = false;
    bool line_irq_low() { return shared_bus.line_irq_low; }
    bool line_nmi_low() { return shared_bus.line_nmi_low; }
    bool prev_line_nmi_low = false;
    bool signal_irq = false;
    bool signal_nmi = false;
    bool should_interrupt = false;
    bool is_interrupt = false;
    bool is_oam_dma_active = false;

    // At normal speed, this will remain accurate for at least 300 millennia
    uint64_t cycle_count = 0;

    void phase_one()
    {
        ++cycle_count;

        ppu.execute_cycle();
        ppu.execute_cycle();
        
        apu.process_frame_cpu_phase();
        
        // End-of-instruction poll result (treat every cycle as the last)
        should_interrupt = signal_irq || signal_nmi;
    }

    void phase_two()
    {
        ppu.execute_cycle();

        apu.process_frame_cpu_phase();
        apu.tick(cycle_count % 2);

        // IRQ level-detector/NMI edge-detector results
        if(!ignore_irq_change)
        {
            signal_irq = line_irq_low() && !(PS & PS_Flags::IRQ_DISABLE);
        }
        if(!ignore_nmi_change)
        {
            signal_nmi = signal_nmi || (line_nmi_low() && !prev_line_nmi_low);
            prev_line_nmi_low = line_nmi_low();
        }
    } 


    // Memory operations
  
    void strobe_controllers(uint8_t data)
    {
        bool strobe = data & (1U << 0);
        port_one.set_strobe(strobe);
        port_two.set_strobe(strobe);
    }


    uint8_t read_reg(uint8_t addr)
    {
        uint8_t data = 0;
        switch(addr)
        {
            case(0x15): data = apu.read_reg_status();  break;
            case(0x16): data = port_one.read_bit();    break;
            case(0x17): data = port_two.read_bit();    break;
            default:    data = 0;                      break;
        }

        return data;
    }

    void write_reg(uint8_t addr, uint8_t data)
    {
        uint8_t sub_addr = addr % 4;
        switch(addr / 4)
        {
            case(0): apu.write_reg_pulse   (sub_addr, data, false);    break;
            case(1): apu.write_reg_pulse   (sub_addr, data, true);     break;
            case(2): apu.write_reg_triangle(sub_addr, data);           break;
            case(3): apu.write_reg_noise   (sub_addr, data);           break;
            //case(4): apu.write_reg_dmc     (sub_addr, data);           break;
            case(5):
            {
                switch(addr % 4)
                {
                    case(0): exec_oam_dma         (data);   break;
                    case(1): apu.write_reg_status(data);   break;
                    case(2): strobe_controllers   (data);   break;
                    case(3): apu.write_reg_frame (data);   break;
                }
                break;
            }
            default: break;
        }
    }

    
    enum class Mem_HW
    {
        RAM,
        PPU_REG,
        IO_REG,
        CART
    };

    using Addr_Info = pair<Mem_HW, uint16_t>;

    Addr_Info parse_addr(uint16_t addr)
    {
        switch(addr / 0x1000)
        {
            case(0x0): case(0x1):  return { Mem_HW::RAM,     addr % 0x800 };
            case(0x2): case(0x3):  return { Mem_HW::PPU_REG, addr % 0x8 };
            default:
                switch((addr - 0x4000) / 0x20)
                {
                    case(0): return { Mem_HW::IO_REG,   addr % 0x20 };
                    default: return { Mem_HW::CART,     addr };
                }
        }
    }

    // These read/write operations assume the CPU/PPU alignment marked 'CPU 3'
    // at http://forums.nesdev.com/viewtopic.php?p=58523#p58523 (as I
    // understand, the read/write logic occurs at the beginning of phase two of
    // the cycle)
    uint8_t mem_read(uint16_t addr)
    {
        phase_one();

        uint8_t data = 0;
        auto [ mem_hw, hw_addr ] = parse_addr(addr);
        switch(mem_hw)
        {
            case(Mem_HW::RAM):     data = ram[hw_addr];
                                   break;
            case(Mem_HW::PPU_REG): data = ppu.read_reg(hw_addr);
                                   break;
            case(Mem_HW::IO_REG):  data = read_reg(hw_addr);
                                   break;
            case(Mem_HW::CART):    data = cart.cpu_read(shared_bus, hw_addr);
                                   break;
        }

        phase_two();

        return data;
    }

    void mem_write(uint16_t addr, uint8_t data)
    {
        phase_one();
        
        auto [ mem_hw, hw_addr ] = parse_addr(addr);
        switch(mem_hw)
        {
            case(Mem_HW::RAM):     ram[hw_addr] = data;             
                                   break;
            case(Mem_HW::PPU_REG): ppu.write_reg(hw_addr, data);   
                                   break;
            case(Mem_HW::IO_REG):  write_reg(hw_addr, data);
                                   break;
            case(Mem_HW::CART):    cart.cpu_write(shared_bus, hw_addr, data);       
                                   break;
        }

        phase_two();
    }

    void exec_oam_dma(uint8_t data)
    {
        is_oam_dma_active = true;

        // What addresses to read from here?
        mem_read(PC);
        if(cycle_count % 2) mem_read(PC);

        for(unsigned int i = 0; i < 0x100; ++i)
        {
            uint8_t value = mem_read((data << 8) | i);
            mem_write(0x2004, value);
        }

        is_oam_dma_active = false;
    }

    uint16_t effective_SP()
    {
        return (0x100U | SP);
    }



    // Operand handling
   
    template<AddrMode am>
    uint8_t read_data(void);
    
    template<AddrMode am>
    void write_data(uint8_t);
    
    template<AddrMode am>
    void get_effective_operand();

    // Helper functions
    void get_effective_operand_ZP_ (uint8_t);
    void get_effective_operand_page_boundary(uint8_t, uint8_t, uint8_t, bool);
    void get_effective_operand_Ab__(uint8_t, bool);
    void get_effective_operand_InY_(bool);



    // Dummy class template used for overload-based dispatch of opcodes
    // (letting operand() take template <Instr, AddrMode> and specializing on
    // Instr would be nice, but partial specialization of function templates is
    // illegal)
    template<Instr> struct Instr_Tag {};


    // Opcodes

    // Common helper function 
    void assign_zn_flags(uint8_t data)
    {
        PS &= ~(PS_Flags::ZERO | 
                PS_Flags::NEGATIVE);

        if(data == 0)
            PS |= PS_Flags::ZERO;

        if(data & (1U << 7))
            PS |= PS_Flags::NEGATIVE;
    }


    // Arithmetic 
    // ADC, SBC
       
    void helper_ADC(uint8_t data)
    {
        bool carry = (PS & PS_Flags::CARRY);
        uint16_t sum = A + data + (carry ? 1U : 0);
        
        PS &= ~PS_Flags::CARRY; 
        if(sum > 0xFF)
            PS |= PS_Flags::CARRY;
        
        assign_zn_flags(sum % 0x100);
       
        // Assign OVERFLOW (V) flag
        // http://forums.nesdev.com/viewtopic.php?t=6331
        PS &= ~PS_Flags::OVERFLOW;
        if((A ^ sum) & (data ^ sum) & 1U << 7)
            PS |= PS_Flags::OVERFLOW;

        A = (sum % 0x100);
    }
     
    // I'm aware that using template-based opcode dispatch leads to a 
    // considerably bigger binary size and heavy cache missing with only
    // the mild benefit of improved inlining; I implemented it this way due
    // to pattern-matching being expressed more easily, and since the CPU
    // isn't a performance bottleneck at the moment, this can be changed later.

    template<AddrMode am>
    void opcode(Instr_Tag<ADC>) 
    { 
        helper_ADC(read_data<am>()); 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SBC>) 
    { 
        // http://forums.nesdev.com/viewtopic.php?f=3&t=8703
        helper_ADC(read_data<am>() ^ 0xFFU); 
    }


    // Logical 
    // AND, ORA, EOR, CMP, CPX, CPY
    
    template<AddrMode am>
    void opcode(Instr_Tag<AND>) 
    { 
        A &= read_data<am>(); 
        assign_zn_flags(A); 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ORA>) 
    { 
        A |= read_data<am>(); 
        assign_zn_flags(A); 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<EOR>) 
    { 
        A ^= read_data<am>(); 
        assign_zn_flags(A); 
    }
    
    void helper_compare(uint8_t reg, uint8_t data)
    {
        PS &= ~PS_Flags::CARRY;
        if(reg >= data) PS |= PS_Flags::CARRY;

        assign_zn_flags(reg - data);
    }

    template<AddrMode am>
    void opcode(Instr_Tag<CMP>) 
    { 
        helper_compare(A, read_data<am>()); 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<CPX>) 
    { 
        helper_compare(X, read_data<am>()); 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<CPY>) 
    { 
        helper_compare(Y, read_data<am>()); 
    }

   
    // Modify 
    // INC, DEC, ASL, LSR, ROL, ROR
    
    template<AddrMode am>
    void opcode(Instr_Tag<INC>)
    {
        uint8_t data = read_data<am>();

        write_data<am>(data);
        ++data;

        assign_zn_flags(data);
        write_data<am>(data);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<DEC>)
    {
        uint8_t data = read_data<am>();

        write_data<am>(data);
        --data;

        assign_zn_flags(data);
        write_data<am>(data);
    }
    
    // While the bulk of the operation usually can't be overlapped and
    // contributes a cycle, the Acc addressing mode is simple enough to allow
    // this
    template<AddrMode am>
    void opcode(Instr_Tag<ASL>)
    {
        uint8_t data = read_data<am>();
        
        PS &= ~PS_Flags::CARRY;
        if(data & (1U << 7))
            PS |= PS_Flags::CARRY;

        if(am != Acc) 
            write_data<am>(data);
        data <<= 1;

        assign_zn_flags(data);
        write_data<am>(data);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LSR>)
    {
        uint8_t data = read_data<am>();
        
        PS &= ~PS_Flags::CARRY;
        if(data & (1U << 0))
            PS |= PS_Flags::CARRY;

        if(am != Acc) 
            write_data<am>(data);
        data >>= 1;

        assign_zn_flags(data);
        write_data<am>(data);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ROL>)
    {
        uint8_t data = read_data<am>();
        bool prev_carry = (PS & PS_Flags::CARRY);

        PS &= ~PS_Flags::CARRY;
        if(data & (1U << 7))
            PS |= PS_Flags::CARRY;

        if(am != Acc) 
            write_data<am>(data);
        data <<= 1;
        data |= (prev_carry ? (1U << 0) : 0);

        assign_zn_flags(data);
        write_data<am>(data);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ROR>)
    {
        uint8_t data = read_data<am>();
        bool prev_carry = (PS & PS_Flags::CARRY);

        PS &= ~PS_Flags::CARRY;
        if(data & (1U << 0))
            PS |= PS_Flags::CARRY;

        if(am != Acc) 
            write_data<am>(data);
        data >>= 1;
        data |= (prev_carry ? (1U << 7) : 0);

        assign_zn_flags(data);
        write_data<am>(data);
    }


    // Control Flow 
    // BPL, BMI, BVC, BVS, BCC, BCS, BNE, BEQ, JMP, JSR, RTS, BRK, RTI, NOP
    
    template<AddrMode am>
    void opcode(Instr_Tag<BPL>) { should_branch = !(PS & PS_Flags::NEGATIVE); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BMI>) { should_branch =  (PS & PS_Flags::NEGATIVE); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BVC>) { should_branch = !(PS & PS_Flags::OVERFLOW); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BVS>) { should_branch =  (PS & PS_Flags::OVERFLOW); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BCC>) { should_branch = !(PS & PS_Flags::CARRY); }   
    
    template<AddrMode am>
    void opcode(Instr_Tag<BCS>) { should_branch =  (PS & PS_Flags::CARRY); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BNE>) { should_branch = !(PS & PS_Flags::ZERO); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BEQ>) { should_branch =  (PS & PS_Flags::ZERO); }
    
    template<AddrMode am>
    void opcode(Instr_Tag<JMP>) 
    {
        // Executed concurrently with previous cycle 
        PC = effective_operand; 
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<JSR>)
    {
        // Strictly speaking, this operation occurs between the fetches of the
        // lsb/msb of the absolute value (new PC); a 'bubble' is required to
        // store the lsb, to allow the old PC to be pushed to the stack
        // (PC is temporarily decremented here to account for here)
        --PC;

        mem_read(effective_SP());
        
        uint8_t msb = PC >> 8;
        mem_write(effective_SP(), msb);
        --SP;
        
        uint8_t lsb = PC % 0x100;
        mem_write(effective_SP(), lsb);
        --SP;
        PC = effective_operand;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<RTS>)
    {
        mem_read(effective_SP());
        ++SP;

        uint8_t lsb = mem_read(effective_SP());
        ++SP;

        uint8_t msb = mem_read(effective_SP());
        PC = ((msb << 8) | lsb);

        mem_read(effective_SP());
        ++PC;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BRK>)
    {
        if(!is_interrupt)
        {
            // Executed concurrently with previous cycle
            ++PC;
        }

        uint8_t msb = (PC >> 8);
        mem_write(effective_SP(), msb);
        --SP;

        uint8_t lsb = (PC % (1U << 8));
        mem_write(effective_SP(), lsb);
        --SP;
        
        // http://wiki.nesdev.com/w/index.php/CPU_interrupts
        // ('Interrupt_hijacking')
        // Can also be hijacked by IRQ (with no effect)
        // Also covers NMI taking precedence over IRQ
        uint16_t interrupt_vector_addr = (signal_nmi
            ? (signal_nmi = false, IV_Addr::NMI)
            : IV_Addr::IRQ);

        uint8_t flags_to_push = PS | PS_Flags::UNUSED;
        if(!is_interrupt) 
            flags_to_push |= PS_Flags::BREAK;
        mem_write(effective_SP(), flags_to_push);
        --SP;
        
        PS |= PS_Flags::IRQ_DISABLE;
        // http://visual6502.org/wiki/index.php?title=6502_Interrupt_Hijacking
        // ('NMI hijacking IRQ/BRK')
        //ignore_nmi_change = true;
        lsb = mem_read(interrupt_vector_addr);
        ++interrupt_vector_addr;

        msb = mem_read(interrupt_vector_addr);
        //ignore_nmi_change = false;
        PC = (msb << 8) | lsb;

        should_interrupt = false;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<RTI>)
    {
        // Dummy read to give time for SP to increment
        mem_read(effective_SP());
        ++SP;

        uint8_t flags_pulled = mem_read(effective_SP());
        flags_pulled &= ~(PS_Flags::UNUSED | PS_Flags::BREAK);
        PS = flags_pulled;
        ++SP;

        uint8_t lsb = mem_read(effective_SP());
        ++SP;

        uint8_t msb = mem_read(effective_SP());
        PC = (msb << 8) | lsb;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<NOP>) {}


    // Flag
    // CLC, SEC, CLD, SED, CLI, SEI, CLV, BIT
    
    template<AddrMode am>
    void opcode(Instr_Tag<CLC>) { PS &= ~PS_Flags::CARRY; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SEC>) { PS |=  PS_Flags::CARRY; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<CLD>) { PS &= ~PS_Flags::DECIMAL; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SED>) { PS |=  PS_Flags::DECIMAL; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<CLI>) { PS &= ~PS_Flags::IRQ_DISABLE; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SEI>) { PS |=  PS_Flags::IRQ_DISABLE; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<CLV>) { PS &= ~PS_Flags::OVERFLOW; }
    
    template<AddrMode am>
    void opcode(Instr_Tag<BIT>)
    {
        uint8_t data = read_data<am>();

        PS &= ~(PS_Flags::ZERO);
        if((data & A) == 0)
            PS |= PS_Flags::ZERO;

        PS &= ~(PS_Flags::OVERFLOW | PS_Flags::NEGATIVE);
        PS |= ((PS_Flags::OVERFLOW | PS_Flags::NEGATIVE) & data);
    }


    // Memory-Register
    // TAX, TAY, TXA, TYA, DEX, DEY, INX, INY, LDA, LDX, LDY, STA, STX, STY
    
    template<AddrMode am>
    void opcode(Instr_Tag<TAX>) 
    { 
        X = A; 
        assign_zn_flags(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<TAY>)
    {
        Y = A;
        assign_zn_flags(Y);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<TXA>)
    {
        A = X;
        assign_zn_flags(A);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<TYA>)
    {
        A = Y;
        assign_zn_flags(A);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<DEX>)
    {
        --X;
        assign_zn_flags(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<DEY>)
    {
        --Y;
        assign_zn_flags(Y);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<INX>)
    {
        ++X;
        assign_zn_flags(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<INY>)
    {
        ++Y;
        assign_zn_flags(Y);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LDA>)
    {
        A = read_data<am>();
        assign_zn_flags(A);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LDX>)
    {
        X = read_data<am>();
        assign_zn_flags(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LDY>)
    {
        Y = read_data<am>();
        assign_zn_flags(Y);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<STA>)
    {
        write_data<am>(A);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<STX>)
    {
        write_data<am>(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<STY>)
    {
        write_data<am>(Y);
    }
   

    // Stack
    // TXS, TSX, PHA, PLA, PHP, PLP
    
    template<AddrMode am>
    void opcode(Instr_Tag<TXS>)
    {
        SP = X;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<TSX>)
    {
        X = SP;
        assign_zn_flags(X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<PHA>)
    {
        mem_write(effective_SP(), A);
        --SP;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<PLA>)
    {
        mem_read(effective_SP());
        ++SP;       

        A = mem_read(effective_SP());
        assign_zn_flags(A);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<PHP>)
    {
        uint8_t flags_to_push = PS | PS_Flags::UNUSED | PS_Flags::BREAK;
        mem_write(effective_SP(), flags_to_push);
        --SP;
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<PLP>)
    {
        mem_read(effective_SP());
        ++SP;       

        uint8_t flags_pulled = mem_read(effective_SP());
        flags_pulled &= ~(PS_Flags::BREAK | PS_Flags::UNUSED);
        PS = flags_pulled;
    }


    // Unofficial
    // TODO implement (not yet cycle accurate!!)
    
    template<AddrMode am>
    void opcode(Instr_Tag<STP>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SLO>) 
    {
        opcode<am>(Instr_Tag<ASL>{});
        opcode<am>(Instr_Tag<ORA>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<RLA>) 
    {
        opcode<am>(Instr_Tag<ROL>{});
        opcode<am>(Instr_Tag<AND>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SRE>) 
    {
        opcode<am>(Instr_Tag<LSR>{});
        opcode<am>(Instr_Tag<EOR>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<RRA>) 
    {
        opcode<am>(Instr_Tag<ROR>{});
        opcode<am>(Instr_Tag<ADC>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SAX>) 
    {
        write_data<am>(A & X);
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LAX>) 
    {
        opcode<am>(Instr_Tag<LDA>{});
        opcode<am>(Instr_Tag<LDX>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<DCP>) 
    {
        opcode<am>(Instr_Tag<DEC>{});
        opcode<am>(Instr_Tag<CMP>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ISC>) 
    {
        opcode<am>(Instr_Tag<INC>{});
        opcode<am>(Instr_Tag<SBC>{});
    }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ANC>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ALR>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<ARR>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<XAA>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<AXS>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<AHX>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<TAS>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<LAS>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SHX>) { }
    
    template<AddrMode am>
    void opcode(Instr_Tag<SHY>) { }


    void reset_state(bool power_cycled)
    {
        // TODO force interrupt lines inactive?

        if(power_cycled)
        {
            A = 0;
            X = 0;
            Y = 0;
            PS = PS_Flags::BREAK | PS_Flags::UNUSED;
            SP = 0;
            PC = 0;
        }

        // RESET interrupt
        // As with other interrupts, follows normal BRK sequence adapted to
        // interrupts, with the addition of surpressing stack writes by reading
        // instead (too cumbersome to express in terms of BRK here)
        // TODO avoid normal reads to avoid affecting other hw clocks?
        //uint8_t msb = (PC >> 8);
        mem_read(effective_SP());
        --SP;

        //uint8_t lsb = (PC % 0x100);
        mem_read(effective_SP());
        --SP;
        
        //uint8_t flags_to_push = PS & ~PS_Flags::BREAK;
        mem_read(effective_SP());
        --SP;
        
        PS |= PS_Flags::IRQ_DISABLE;
        uint16_t interrupt_vector_addr = IV_Addr::RESET;
        uint8_t lsb = mem_read(interrupt_vector_addr);
        ++interrupt_vector_addr;

        uint8_t msb = mem_read(interrupt_vector_addr);
        PC = (msb << 8) | lsb;
    }

    // Precondition: a branch with a satisfied condition was executed just prior
    void perform_branch()
    {
        // Convert two's complement unsigned byte to signed byte
        // (on most systems (int8_t)effective_operand would suffice, but this
        // ensures portability)
        int displacement = (((int)effective_operand + 0x80) % 0x100) - 0x80;

        uint8_t lsb = PC % 0x100;
        uint8_t msb = PC >> 8;

        // http://wiki.nesdev.com/w/index.php/CPU_interrupts ('Branch 
        // instructions and interrupts')
        // Interrupt lines are not polled on this cycle
        ignore_irq_change = ignore_nmi_change = true;
        mem_read(PC);
        ignore_irq_change = ignore_nmi_change = false;
        uint16_t new_PC = PC + displacement;

        // Page boundary check
        if((PC / 0x100) != (new_PC / 0x100))
        {
            // A memory read occurred on the wrong page
            mem_read((msb << 8) | (lsb + displacement));
        }

        PC = new_PC;
    }


    template<Instr i, AddrMode am>
    void op()
    {
        get_effective_operand<am>(); 
        should_branch = false;

        opcode<am>(Instr_Tag<i>{});
        
        if(should_branch)
            perform_branch();
    }


  public:
    
    CPU(Shared_Bus& shared_bus, Cartridge& cart, PPU& ppu, APU& apu, 
            Controller& port_one, Controller& port_two)
        : shared_bus(shared_bus), cart(cart), ppu(ppu), apu(apu), 
          port_one(port_one), port_two(port_two)
    {
        reset_state(true);
    }

    uint64_t get_cycle_count() { return cycle_count; }
    
    void execute_instruction()
    {
        using C = CPU;
        using Func = void(C::*)();

        static constexpr Func dispatch_table[0x100] = 
        {
          //+0x00            +0x01            +0x02            +0x03
/*0x00*/    &C::op<BRK,Imp>, &C::op<ORA,InX>, &C::op<STP,Imp>, &C::op<SLO,InX>, 
/*0x04*/    &C::op<NOP,ZP>,  &C::op<ORA,ZP>,  &C::op<ASL,ZP>,  &C::op<SLO,ZP>,  
/*0x08*/    &C::op<PHP,Imp>, &C::op<ORA,Imm>, &C::op<ASL,Acc>, &C::op<ANC,Imm>, 
/*0x0C*/    &C::op<NOP,Ab>,  &C::op<ORA,Ab>,  &C::op<ASL,Ab>,  &C::op<SLO,Ab>,  
/*0x10*/    &C::op<BPL,Imm>, &C::op<ORA,InY>, &C::op<STP,Imp>, &C::op<SLO,InYS>, 
/*0x14*/    &C::op<NOP,ZPX>, &C::op<ORA,ZPX>, &C::op<ASL,ZPX>, &C::op<SLO,ZPX>, 
/*0x18*/    &C::op<CLC,Imp>, &C::op<ORA,AbY>, &C::op<NOP,Imp>, &C::op<SLO,AbYS>, 
/*0x1C*/    &C::op<NOP,AbX>, &C::op<ORA,AbX>, &C::op<ASL,AbXS>,&C::op<SLO,AbXS>, 
/*0x20*/    &C::op<JSR,Ab>,  &C::op<AND,InX>, &C::op<STP,Imp>, &C::op<RLA,InX>, 
/*0x24*/    &C::op<BIT,ZP>,  &C::op<AND,ZP>,  &C::op<ROL,ZP>,  &C::op<RLA,ZP>,  
/*0x28*/    &C::op<PLP,Imp>, &C::op<AND,Imm>, &C::op<ROL,Acc>, &C::op<ANC,Imm>, 
/*0x2C*/    &C::op<BIT,Ab>,  &C::op<AND,Ab>,  &C::op<ROL,Ab>,  &C::op<RLA,Ab>,  
/*0x30*/    &C::op<BMI,Imm>, &C::op<AND,InY>, &C::op<STP,Imp>, &C::op<RLA,InYS>, 
/*0x34*/    &C::op<NOP,ZPX>, &C::op<AND,ZPX>, &C::op<ROL,ZPX>, &C::op<RLA,ZPX>, 
/*0x38*/    &C::op<SEC,Imp>, &C::op<AND,AbY>, &C::op<NOP,Imp>, &C::op<RLA,AbYS>, 
/*0x3C*/    &C::op<NOP,AbX>, &C::op<AND,AbX>, &C::op<ROL,AbXS>,&C::op<RLA,AbXS>, 
/*0x40*/    &C::op<RTI,Imp>, &C::op<EOR,InX>, &C::op<STP,Imp>, &C::op<SRE,InX>, 
/*0x44*/    &C::op<NOP,ZP>,  &C::op<EOR,ZP>,  &C::op<LSR,ZP>,  &C::op<SRE,ZP>,  
/*0x48*/    &C::op<PHA,Imp>, &C::op<EOR,Imm>, &C::op<LSR,Acc>, &C::op<ALR,Imm>, 
/*0x4C*/    &C::op<JMP,Ab>,  &C::op<EOR,Ab>,  &C::op<LSR,Ab>,  &C::op<SRE,Ab>,  
/*0x50*/    &C::op<BVC,Imm>, &C::op<EOR,InY>, &C::op<STP,Imp>, &C::op<SRE,InYS>, 
/*0x54*/    &C::op<NOP,ZPX>, &C::op<EOR,ZPX>, &C::op<LSR,ZPX>, &C::op<SRE,ZPX>, 
/*0x58*/    &C::op<CLI,Imp>, &C::op<EOR,AbY>, &C::op<NOP,Imp>, &C::op<SRE,AbYS>, 
/*0x5C*/    &C::op<NOP,AbX>, &C::op<EOR,AbX>, &C::op<LSR,AbXS>,&C::op<SRE,AbXS>, 
/*0x60*/    &C::op<RTS,Imp>, &C::op<ADC,InX>, &C::op<STP,Imp>, &C::op<RRA,InX>, 
/*0x64*/    &C::op<NOP,ZP>,  &C::op<ADC,ZP>,  &C::op<ROR,ZP>,  &C::op<RRA,ZP>,  
/*0x68*/    &C::op<PLA,Imp>, &C::op<ADC,Imm>, &C::op<ROR,Acc>, &C::op<ARR,Imm>, 
/*0x6C*/    &C::op<JMP,In>,  &C::op<ADC,Ab>,  &C::op<ROR,Ab>,  &C::op<RRA,Ab>,  
/*0x70*/    &C::op<BVS,Imm>, &C::op<ADC,InY>, &C::op<STP,Imp>, &C::op<RRA,InYS>, 
/*0x74*/    &C::op<NOP,ZPX>, &C::op<ADC,ZPX>, &C::op<ROR,ZPX>, &C::op<RRA,ZPX>, 
/*0x78*/    &C::op<SEI,Imp>, &C::op<ADC,AbY>, &C::op<NOP,Imp>, &C::op<RRA,AbYS>, 
/*0x7C*/    &C::op<NOP,AbX>, &C::op<ADC,AbX>, &C::op<ROR,AbXS>,&C::op<RRA,AbXS>, 
/*0x80*/    &C::op<NOP,Imm>, &C::op<STA,InX>, &C::op<NOP,Imm>, &C::op<SAX,InX>, 
/*0x84*/    &C::op<STY,ZP>,  &C::op<STA,ZP>,  &C::op<STX,ZP>,  &C::op<SAX,ZP>,  
/*0x88*/    &C::op<DEY,Imp>, &C::op<NOP,Imm>, &C::op<TXA,Imp>, &C::op<XAA,Imm>, 
/*0x8C*/    &C::op<STY,Ab>,  &C::op<STA,Ab>,  &C::op<STX,Ab>,  &C::op<SAX,Ab>,  
/*0x90*/    &C::op<BCC,Imm>, &C::op<STA,InYS>,&C::op<STP,Imp>, &C::op<AHX,InY>, 
/*0x94*/    &C::op<STY,ZPX>, &C::op<STA,ZPX>, &C::op<STX,ZPY>, &C::op<SAX,ZPY>, 
/*0x98*/    &C::op<TYA,Imp>, &C::op<STA,AbYS>,&C::op<TXS,Imp>, &C::op<TAS,AbY>, 
/*0x9C*/    &C::op<SHY,AbX>, &C::op<STA,AbXS>,&C::op<SHX,AbY>, &C::op<AHX,AbY>, 
/*0xA0*/    &C::op<LDY,Imm>, &C::op<LDA,InX>, &C::op<LDX,Imm>, &C::op<LAX,InX>, 
/*0xA4*/    &C::op<LDY,ZP>,  &C::op<LDA,ZP>,  &C::op<LDX,ZP>,  &C::op<LAX,ZP>,  
/*0xA8*/    &C::op<TAY,Imp>, &C::op<LDA,Imm>, &C::op<TAX,Imp>, &C::op<LAX,Imm>, 
/*0xAC*/    &C::op<LDY,Ab>,  &C::op<LDA,Ab>,  &C::op<LDX,Ab>,  &C::op<LAX,Ab>,  
/*0xB0*/    &C::op<BCS,Imm>, &C::op<LDA,InY>, &C::op<STP,Imp>, &C::op<LAX,InY>, 
/*0xB4*/    &C::op<LDY,ZPX>, &C::op<LDA,ZPX>, &C::op<LDX,ZPY>, &C::op<LAX,ZPY>, 
/*0xB8*/    &C::op<CLV,Imp>, &C::op<LDA,AbY>, &C::op<TSX,Imp>, &C::op<LAS,AbY>, 
/*0xBC*/    &C::op<LDY,AbX>, &C::op<LDA,AbX>, &C::op<LDX,AbY>, &C::op<LAX,AbY>, 
/*0xC0*/    &C::op<CPY,Imm>, &C::op<CMP,InX>, &C::op<NOP,Imm>, &C::op<DCP,InX>, 
/*0xC4*/    &C::op<CPY,ZP>,  &C::op<CMP,ZP>,  &C::op<DEC,ZP>,  &C::op<DCP,ZP>,  
/*0xC8*/    &C::op<INY,Imp>, &C::op<CMP,Imm>, &C::op<DEX,Imp>, &C::op<AXS,Imm>, 
/*0xCC*/    &C::op<CPY,Ab>,  &C::op<CMP,Ab>,  &C::op<DEC,Ab>,  &C::op<DCP,Ab>,  
/*0xD0*/    &C::op<BNE,Imm>, &C::op<CMP,InY>, &C::op<STP,Imp>, &C::op<DCP,InYS>,
/*0xD4*/    &C::op<NOP,ZPX>, &C::op<CMP,ZPX>, &C::op<DEC,ZPX>, &C::op<DCP,ZPX>, 
/*0xD8*/    &C::op<CLD,Imp>, &C::op<CMP,AbY>, &C::op<NOP,Imp>, &C::op<DCP,AbYS>, 
/*0xDC*/    &C::op<NOP,AbX>, &C::op<CMP,AbX>, &C::op<DEC,AbXS>,&C::op<DCP,AbXS>, 
/*0xE0*/    &C::op<CPX,Imm>, &C::op<SBC,InX>, &C::op<NOP,Imm>, &C::op<ISC,InX>, 
/*0xE4*/    &C::op<CPX,ZP>,  &C::op<SBC,ZP>,  &C::op<INC,ZP>,  &C::op<ISC,ZP>,  
/*0xE8*/    &C::op<INX,Imp>, &C::op<SBC,Imm>, &C::op<NOP,Imp>, &C::op<SBC,Imm>, 
/*0xEC*/    &C::op<CPX,Ab>,  &C::op<SBC,Ab>,  &C::op<INC,Ab>,  &C::op<ISC,Ab>,  
/*0xF0*/    &C::op<BEQ,Imm>, &C::op<SBC,InY>, &C::op<STP,Imp>, &C::op<ISC,InYS>, 
/*0xF4*/    &C::op<NOP,ZPX>, &C::op<SBC,ZPX>, &C::op<INC,ZPX>, &C::op<ISC,ZPX>, 
/*0xF8*/    &C::op<SED,Imp>, &C::op<SBC,AbY>, &C::op<NOP,Imp>, &C::op<ISC,AbYS>, 
/*0xFC*/    &C::op<NOP,AbX>, &C::op<SBC,AbX>, &C::op<INC,AbXS>,&C::op<ISC,AbXS>, 
        };

        uint8_t opcode = mem_read(PC++);
        (this->*(dispatch_table[opcode]))();
        
        if(should_interrupt)
        {
            // Dummy read the next opcode and discard it (inserting BRK into the
            // instruction register) to allow overlapped final cycle of previous
            // instruction to complete, if necessary
            mem_read(PC);

            is_interrupt = true;
            op<BRK,Imp>();
            is_interrupt = false;
        }
    }
};




template<> inline void CPU::get_effective_operand<Imp>() 
{
    // First byte of 'operand' (which does not exist) is read/discarded
    mem_read(PC); 
    effective_operand = 0; 
}

template<> inline void CPU::get_effective_operand<Acc>() 
{
    // First byte of 'operand' (which does not exist) is read/discarded
    mem_read(PC);
    effective_operand = A; 
}

template<> inline void CPU::get_effective_operand<Imm>()
{ 
    uint8_t immediate = mem_read(PC++);
    effective_operand = immediate;
}

template<> inline void CPU::get_effective_operand<ZP>()
{
    uint8_t index = mem_read(PC++);
    effective_operand = index;
}


inline void CPU::get_effective_operand_ZP_(uint8_t reg)
{
    uint8_t index = mem_read(PC++);
    
    mem_read(index);
    index += reg;
    effective_operand = index;
}

template<> inline void CPU::get_effective_operand<ZPX>() 
{ 
    get_effective_operand_ZP_(X); 
}

template<> inline void CPU::get_effective_operand<ZPY>()
{
   get_effective_operand_ZP_(Y); 
}


template<> inline void CPU::get_effective_operand<Ab>()
{
    uint8_t lsb = mem_read(PC++);

    uint8_t msb = mem_read(PC++);
    uint16_t index = (msb << 8) | lsb;
    effective_operand = index;
}

inline void CPU::get_effective_operand_page_boundary(uint8_t lsb, uint8_t msb, 
        uint8_t reg, bool is_write_involved)
{
    uint16_t index;
    lsb += reg;
    bool carry_occurred = (lsb < reg);
    index = (msb << 8) | lsb;

    if(is_write_involved || carry_occurred)
    {
        // Take the extra cycle to add the register to the base index correctly
        mem_read(index);
        if(carry_occurred) index += 0x100;
    }
    // Ultimately, index == ((msb << 8) | lsb) + reg
    effective_operand = index;
}

inline void CPU::get_effective_operand_Ab__(uint8_t reg, bool is_write_involved)
{
    uint8_t lsb = mem_read(PC++);
    
    uint8_t msb = mem_read(PC++);

    get_effective_operand_page_boundary(lsb, msb, reg, is_write_involved);
}

template<> inline void CPU::get_effective_operand<AbX>()
{
    get_effective_operand_Ab__(X, false);
}

template<> inline void CPU::get_effective_operand<AbXS>()
{
    get_effective_operand_Ab__(X, true);
}

template<> inline void CPU::get_effective_operand<AbY>()
{
    get_effective_operand_Ab__(Y, false);
}

template<> inline void CPU::get_effective_operand<AbYS>()
{
    get_effective_operand_Ab__(Y, true);
}


template<> inline void CPU::get_effective_operand<In>()
{
    uint8_t fst_lsb = mem_read(PC++);

    uint8_t fst_msb = mem_read(PC++);
    uint16_t index = (fst_msb << 8) | fst_lsb;

    uint8_t snd_lsb = mem_read(index);
    ++fst_lsb;
    index = (fst_msb << 8) | fst_lsb;

    uint8_t snd_msb = mem_read(index);
    index = (snd_msb << 8) | snd_lsb;
    effective_operand = index;
}

template<> inline void CPU::get_effective_operand<InX>()
{
    uint8_t index = mem_read(PC++);

    mem_read(index);
    index += X;

    uint8_t lsb = mem_read(index);

    ++index;
    uint8_t msb = mem_read(index);
    uint16_t new_index = (msb << 8) | lsb;
    effective_operand = new_index;
}

inline void CPU::get_effective_operand_InY_(bool is_write_involved)
{
    uint8_t index = mem_read(PC++);

    uint8_t lsb = mem_read(index);

    ++index;
    uint8_t msb = mem_read(index);

    get_effective_operand_page_boundary(lsb, msb, Y, is_write_involved);
}

template<> inline void CPU::get_effective_operand<InY>()
{
    get_effective_operand_InY_(false);
}

template<> inline void CPU::get_effective_operand<InYS>()
{
    get_effective_operand_InY_(true);
}




template<AddrMode am>
uint8_t CPU::read_data() { return mem_read(effective_operand); }

template<> inline uint8_t CPU::read_data<Acc>() { return effective_operand; }
template<> inline uint8_t CPU::read_data<Imm>() { return effective_operand; }
template<> inline uint8_t CPU::read_data<In>()  { return effective_operand; }

// For the Imp addressing mode, 'reading' makes no sense
template<> inline uint8_t CPU::read_data<Imp>() = delete;




template<AddrMode am>
void CPU::write_data(uint8_t data) { mem_write(effective_operand, data); }

template<> inline void CPU::write_data<Acc>(uint8_t data) { A = data; }

// For the Imp/Imm addressing modes, 'writing' makes no sense
template<> inline void CPU::write_data<Imp>(uint8_t) = delete;
template<> inline void CPU::write_data<Imm>(uint8_t) = delete;

// Writing with these addressing modes (the non-__S variants, i.e. not
// safeguarded against page-boundary optimisation) is forbidden
template<> inline void CPU::write_data<AbX>(uint8_t) = delete;
template<> inline void CPU::write_data<AbY>(uint8_t) = delete;
template<> inline void CPU::write_data<InY>(uint8_t) = delete;


}

#endif //CPU_H_NOS
