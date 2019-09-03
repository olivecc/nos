#ifndef  CART_H_NOS
#define  CART_H_NOS

#include "shared_bus.h"

namespace NES
{


class Cartridge
{
  public:
    virtual uint8_t ppu_read (Shared_Bus&, uint16_t addr) = 0; 
    virtual void    ppu_write(Shared_Bus&, uint16_t addr, uint8_t data) = 0;

    virtual uint8_t cpu_read (Shared_Bus&, uint16_t addr) = 0;
    virtual void    cpu_write(Shared_Bus&, uint16_t addr, uint8_t data) = 0;
};


}

#endif //CART_H_NOS
