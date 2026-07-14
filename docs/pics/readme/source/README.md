# OLED README Preview Renderer

This documentation-only host renderer produces the 64×48 OLED gallery used by
the root README.

It compiles against the same U8g2 C source and font arrays installed by
PlatformIO for the firmware project. It also extracts the selected weather,
room, measure, and timer XBM arrays directly from
`firmware/src/display_manager.cpp`.

## Prerequisites

- MSYS2 UCRT64 GCC (`C:\msys64\ucrt64\bin\gcc.exe`)
- Python with Pillow

## Regenerate

From the repository root:

```powershell
python docs/pics/readme/source/extract_xbm.py
& 'C:\msys64\usr\bin\bash.exe' -lc 'export PATH=/ucrt64/bin:$PATH; cd /c/Users/wangbo/Desktop/Work/"Personal Repo"/Aether-OS; mkdir -p docs/pics/readme/generated; gcc -O2 -I firmware/.pio/libdeps/esp32dev/U8g2/src/clib -I docs/pics/readme/source docs/pics/readme/source/u8g2_render.c firmware/.pio/libdeps/esp32dev/U8g2/src/clib/*.c -o docs/pics/readme/source/build/u8g2_render.exe; docs/pics/readme/source/build/u8g2_render.exe docs/pics/readme/generated'
python docs/pics/readme/source/assemble_oled_gallery.py
```

`generated/`, the compiled executable, and `generated_icons.h` are ignored:
they are reproducible intermediates. Commit only
`docs/pics/readme/oled-ui-gallery.png` after reviewing it.
