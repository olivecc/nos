# NOS

NOS is a cross-platform emulator for the NTSC revisions of the _Nintendo
Entertainment System_ and _Family Computer_ video game consoles released by
Nintendo Co., Ltd. in the early 1980s, henceforth collectively referred to as
the NES.

Focus is placed on accuracy of both emulation and implementation; that is, the
source code should reflect the internal operation of NES hardware (or at least
explicitly document when impractical to implement), so that the source code
can document how a NES works for those interested. The emulator core is
implemented as a machine code interpreter; I'd rather have a solid
understanding of how the NES works in detail before I attempt to implement
[dynamic recompilation][dynarec].

The core is so far implemented as a header-only library; while separation into
source files would reflect better practice, function inlining and optimization
are prioritised for execution speed concerns, given that some functions need to
be called millions of times per second.

The core is written in C++ with no external dependencies (aside from the
Standard Library). A simple cross-platform test program and I/O library for
making use of the emulator core are provided as well, written in C++ and based
on the [Simple DirectMedia Layer][sdl] library (specifically SDL2).

This emulator is under construction.


## Compatibility

As NOS is being developed to emulate the NTSC revisions of the NES, please do
not expect PAL games to work.

My ability to test with commercial games (as opposed to homebrew/test programs)
is limited by my ability to acquire NES cartridges and dump/make backups of
them, so please let me know if you encounter issues using NOS with particular
games.

Note that downloading commercial games from the Web is generally illegal in most
countries; I do not encourage or tolerate this practice, and accept no liability
for legal action resulting from doing so. Instead, I'd like to note that the
process of making backups of one's own NES cartridges has many affordable
solutions ([example][rom-dump]) with little technical expertise required.  


## Build

Currently the only build process available for the test program is the
`compile.sh` test script under `test/`, for use on Linux. Cobbling together
alternatives for other platforms should be easy enough, subject to compatibility
with SDL2.

## Author

**Oliver Vecchini** [olivecc](https://github.com/olivecc)  
License information can be found [here](LICENSE).


[dynarec]:
https://en.wikipedia.org/wiki/Dynamic_recompilation

[sdl]:
https://wiki.libsdl.org/FrontPage

[rom-dump]:
https://web.archive.org/web/20190719112157/https://arekuse.net/blog/tech-guides/rom-dumping-and-hacking/rom-dumping-nes/  
