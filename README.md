<a rel="license" href="http://creativecommons.org/licenses/by/4.0/"><img alt="Creative Commons Licence" style="border-width:0" src="https://i.creativecommons.org/l/by/4.0/80x15.png" /></a><br /><span xmlns:dct="http://purl.org/dc/terms/" href="http://purl.org/dc/dcmitype/InteractiveResource" property="dct:title" rel="dct:type">Aaron Oman's CHIP-8 Emulator</span> by <a xmlns:cc="http://creativecommons.org/ns#" href="https://code.groovestomp.com/chip8/" property="cc:attributionName" rel="cc:attributionURL">Aaron Oman</a> is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by/4.0/">Creative Commons Attribution 4.0 International License</a>.

NOTE: This license does not extend to any of the programs in the `games/` directory.
I will update this README appropriately when I have more information about those licenses.

More Details coming soon.

# Dependencies
- sdl2
- OpenGL 3
- GLEW (OpenGL Extension Wrangler)
- libmath
- libpthread
- soundio

# Build
```
make
```

# Run
Where `$FILE` is one of the premade games in the `games/` directory.
```
./build/chip8 games/$FILE
```