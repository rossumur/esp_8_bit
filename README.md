# Color composite video generator from **ESP_8_BIT**

This project started with __rossumer__'s
[ESP_8_BIT project](https://github.com/rossumur/esp_8_bit)
and cut away everything unrelated to generating a color composite video signal.

ESP32 GPIO25 is the video signal output pin.

Frame buffer for the video output routineis given as `_lines` which is an array
of 240 `uint8_t*` each reprenting a line of data. Each line is an array of 256
`uint8_t` values. Each of which is an 8-bit color in
[RGB332 format](https://en.wikipedia.org/wiki/List_of_monochrome_and_RGB_color_formats#8-bit_RGB_(also_known_as_3-3-2_bit_RGB))
as specified by `ntsc_rgb332`.

This project includes a simple test case, displaying a full-screen bitmap
described by the RGB332 numbers in the `jubsRGB332` array. This array was generated
by the script in `\util` directory, and the 256x240 PNG source file is also there
to serve as example.

----------

My target platform is the dual-core ESP32 for NTSC televisions.

During the trimming process I removed some code explicitly supporting the single-
core ESP32-S2, though it should be easy to add them back if needed.

I tried to preserve all PAL functionality, but I may have inadvertently broken
PAL support. I have no PAL TV to test against.

----------

## Thanks to *rossumur* who worked out all the color composite video generation
## code, without which this project would not have been possible.
