#ifndef  INES_H_NOS
#define  INES_H_NOS

#include <cstdint>      // uint8_t
#include <memory>       // unique_ptr
#include <vector>

#include "cart.h"


std::unique_ptr<NES::Cartridge> load_ines(const std::vector<uint8_t>&);


#endif //INES_H_NOS
