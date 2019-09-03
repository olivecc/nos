#include <cstdint>      // uint8_t, uint32_t
#include <vector>

#include <fstream>
#include <iterator>
#include <iostream>
#include <cstdlib>

#include "console.h"
#include "SDL.h"
#include "sdl_aux.h"
#include "ines.h"

using namespace NES;

using std::vector;

static constexpr size_t sample_rate = 44100;
static constexpr size_t frame_rate = 60;

//Precondition: for each uint8_t val in src, val < 0x40
void convert_framebuf(const uint8_t (&src)[pixel_quantity], 
                           uint32_t (&dst)[pixel_quantity])
{
    static constexpr uint32_t palette_table[0x40] = 
    {
        0xFF545454, 0xFF001E74, 0xFF081090, 0xFF300088,
        0xFF440064, 0xFF5C0030, 0xFF540400, 0xFF3C1800,
        0xFF202A00, 0xFF083A00, 0xFF004000, 0xFF003C00,
        0xFF00323C, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFF989698, 0xFF084CC4, 0xFF3032EC, 0xFF5C1EE4,
        0xFF8814B0, 0xFFA01464, 0xFF982220, 0xFF783C00,
        0xFF545A00, 0xFF287200, 0xFF087C00, 0xFF007628,
        0xFF006678, 0xFF000000, 0xFF000000, 0xFF000000,
        0xFFECEEEC, 0xFF4C9AEC, 0xFF787CEC, 0xFFB062EC,
        0xFFE454EC, 0xFFEC58B4, 0xFFEC6A64, 0xFFD48820,
        0xFFA0AA00, 0xFF74C400, 0xFF4CD020, 0xFF38CC6C,
        0xFF38B4CC, 0xFF3C3C3C, 0xFF000000, 0xFF000000,
        0xFFECEEEC, 0xFFA8CCEC, 0xFFBCBCEC, 0xFFD4B2EC,
        0xFFECAEEC, 0xFFECAED4, 0xFFECB4B0, 0xFFE4C490,
        0xFFCCD278, 0xFFB4DE78, 0xFFA8E290, 0xFF98E2B4,
        0xFFA0D6E4, 0xFFA0A2A0, 0xFF000000, 0xFF000000 
    };

    for(unsigned int i = 0; i < pixel_quantity; ++i)
    {
        dst[i] = palette_table[src[i]];
    }
}

// Precondition: dst_len < src_len
void resample_audio(const float* src, const size_t src_len,  
                          float* dst, const size_t dst_len)
{
    double sum = 0;
    size_t samples_src_per_dst = 0;
    size_t num = 0;
    
    size_t dst_i = 0;

    for(size_t src_i = 0; src_i < src_len; ++src_i)
    {
        sum += src[src_i];
        ++samples_src_per_dst;
        num += dst_len;
        
        if(num >= src_len)
        {
            dst[dst_i++] = (sum / samples_src_per_dst);

            sum = 0;
            samples_src_per_dst = 0;
            num -= src_len;
        }
    }
}

vector<uint8_t> load_file(const char* path)
{
    std::ifstream input(path, std::ios::binary | std::ios::in);
    input >> std::noskipws;
    return vector<uint8_t>(std::istream_iterator<uint8_t>(input), {});
}

void run(const SDL_Aux::State& io, const char* rom_filepath)
{
    vector<uint8_t> rom = load_file(rom_filepath);
    Console console(load_ines(rom));

    uint32_t argb_framebuf[width_px * height_px];
    size_t samples_out_per_frame = sample_rate / frame_rate;
    float audio_out[samples_out_per_frame];
    
    uint64_t frame_count = 0;

    do 
    {
        console.exec();

        if(console.get_frame_count() != frame_count)
        {
            convert_framebuf(console.get_framebuf(), argb_framebuf);
            SDL_Aux::render(io, argb_framebuf, width_px);

            resample_audio(console.get_audiobuf(), max_samples_per_frame,
                           audio_out,              samples_out_per_frame);
            SDL_QueueAudio(io.audio_device, audio_out, sizeof(audio_out));
                           

            frame_count = console.get_frame_count();
            
            SDL_PumpEvents();
            using B = Controller::Button;
            console.set_port_one(B::A,      io.kb_state[SDL_SCANCODE_H]);
            console.set_port_one(B::B,      io.kb_state[SDL_SCANCODE_J]);
            console.set_port_one(B::SELECT, io.kb_state[SDL_SCANCODE_F]);
            console.set_port_one(B::START,  io.kb_state[SDL_SCANCODE_G]);
            console.set_port_one(B::UP,     io.kb_state[SDL_SCANCODE_W]);
            console.set_port_one(B::DOWN,   io.kb_state[SDL_SCANCODE_S]);
            console.set_port_one(B::LEFT,   io.kb_state[SDL_SCANCODE_A]);
            console.set_port_one(B::RIGHT,  io.kb_state[SDL_SCANCODE_D]);
        }
    } 
    while(!(io.kb_state[SDL_SCANCODE_ESCAPE]));
}

int main(int argc, char** argv)
{
    if(argc != 2) return 1;
    const char* rom_filepath = argv[1];

    SDL_Aux::State io;
    SDL_Aux::init(io, width_px, height_px, sample_rate);
    
    run(io, rom_filepath);
    
    SDL_Aux::quit(io);
}
