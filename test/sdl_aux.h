#ifndef  SDL_AUX_H
#define  SDL_AUX_H

#include <climits>
#include <stdexcept>
#include "SDL.h"

static_assert(CHAR_BIT == 8);

namespace SDL_Aux
{


struct State
{
    const Uint8*      kb_state;
    SDL_Window*       window;
    SDL_Renderer*     renderer;
    SDL_Texture*      texture;
    SDL_AudioDeviceID audio_device;
};

inline void init(State& state, unsigned int width, unsigned int height, 
    int sample_rate)
{
    auto init_fail = [](){ throw std::runtime_error("Failed to init SDL"); };

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    enum { ANY_DRIVER = -1, NO_FLAGS = 0 };


    const char* nearest_pixel_sampling = "0"; 
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, nearest_pixel_sampling);
    
    auto window = SDL_CreateWindow("test", 0, 0, width, height,
        SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(window == NULL) init_fail();
    
    auto renderer = SDL_CreateRenderer(window, ANY_DRIVER, 
        SDL_RENDERER_PRESENTVSYNC);
        //NO_FLAGS);
    if(renderer == NULL) init_fail();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);

    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    if(texture == NULL) init_fail();


    static_assert(sizeof(float) == 4);
    SDL_AudioSpec audio_spec = 
    {
        .freq = sample_rate,
        .format = AUDIO_F32,
        .channels = 1,
        .samples = 2048,
        .callback = NULL
    };
    SDL_AudioSpec dummy; // Unused, but required for SDL
    auto audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, &dummy, 
        NO_FLAGS);
    if(audio_device == 0) init_fail();
    SDL_PauseAudioDevice(audio_device, 0);

    state = {
        SDL_GetKeyboardState(NULL),
        window,
        renderer,
        texture,
        audio_device
    };
}

inline void quit(State& io)
{
    SDL_DestroyTexture (io.texture);
    SDL_DestroyRenderer(io.renderer);
    SDL_DestroyWindow  (io.window);

    SDL_Quit();
}

inline void render(const State& io, uint32_t* buffer, unsigned int width)
{
    static_assert(sizeof(Uint32) == sizeof(uint32_t));

    SDL_UpdateTexture(io.texture, NULL, buffer, width * sizeof(Uint32)); 
    SDL_RenderClear  (io.renderer);
    SDL_RenderCopy   (io.renderer, io.texture, NULL, NULL);
    SDL_RenderPresent(io.renderer);
}


}

#endif //SDL_AUX_H
