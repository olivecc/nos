#ifndef  MAPPER_IMPL_H_NOS
#define  MAPPER_IMPL_H_NOS

#include "header.h"
#include "mapper.h"

template<unsigned int PRG_BSE, unsigned int CHR_BSE, 
    unsigned int PRG_BNUM_MIN = 1>
class Mapper_Impl : public Mapper
{
  protected:
    unsigned int get_prg_bank_size_exp() override final { return PRG_BSE; }
    unsigned int get_chr_bank_size_exp() override final { return CHR_BSE; }

  public:
    Mapper_Impl(const Header& header) : Mapper(header)
    {
        if(get_prg_bank_num() < PRG_BNUM_MIN)
            throw std::runtime_error("Not enough PRG-ROM");
    }
};

#endif //MAPPER_IMPL_H_NOS
