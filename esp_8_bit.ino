/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "esp_system.h"
#include "esp_int_wdt.h"

#define PERF  // some stats about where we spend our time
#include "src/video_out.h"
#include "src/jubs332.h"

//  True for NTSC, False for PAL
#define NTSC_VIDEO true

uint32_t _frame_time = 0;
uint32_t _drawn = 1;
bool _inited = false;

uint8_t** _bufLines;

void frame_generation(void* arg)
{
    printf("Frame generation running on core %d at %dmhz\n",
      xPortGetCoreID(),rtc_clk_cpu_freq_value(rtc_clk_cpu_freq_get()));
    _drawn = _frame_counter;
    while(true)
    {
      // wait for blanking before drawing to avoid tearing
      video_sync();

      // Draw a frame
      uint32_t t = xthal_get_ccount();
      _frame_time = xthal_get_ccount() - t;
      _lines = _bufLines;
      _drawn++;
    }
}

void setup()
{ 
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_240M);  

  xTaskCreatePinnedToCore(frame_generation, "frame_generation", 3*1024, NULL, 0, NULL, 0);

  _bufLines = new uint8_t*[240];
  uint8_t* linePointer = jubsRGB332;
  for (int y = 0; y < 240; y++)
  {
    _bufLines[y] = linePointer;
    linePointer += 256;
  }
}

#ifdef PERF
void perf()
{
  static int _next = 0;
  if (_drawn >= _next) {
    float elapsed_us = 120*1000000/(NTSC_VIDEO ? 60 : 50);
    _next = _drawn + 120;
    
    printf("frame_time:%d drawn:%d displayed:%d blit_ticks:%d->%d, isr time:%2.2f%%\n",
      _frame_time/240,_drawn,_frame_counter,_blit_ticks_min,_blit_ticks_max,(_isr_us*100)/elapsed_us);
      
    _blit_ticks_min = 0xFFFFFFFF;
    _blit_ticks_max = 0;
    _isr_us = 0;
  }
}
#else
void perf(){};
#endif

// this loop always runs on app_core (1).
void loop()
{    
  // start the video after emu has started
  if (!_inited) {
    if (_lines) {
      printf("video_init\n");
      video_init(4,NTSC_VIDEO); // start the A/V pump
      _inited = true;
    } else {
      vTaskDelay(1);
    }
  }
  
  // Dump some stats
  perf();
}
