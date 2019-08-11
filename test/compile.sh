g++ -I ../core ./*.cpp -std=c++17 -lSDL2 $(sdl2-config --cflags) -Wno-overflow -o nos -O3
