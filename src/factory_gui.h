#pragma once

#define UI_BG_COLOR     lv_color_black()
#define UI_FG_COLOR     lv_color_white()
#define UI_PAGE_COUNT   1

#define LV_DELAY(x)                                                                                                                                  \
  do {                                                                                                                                               \
    uint32_t t = x;                                                                                                                                  \
    while (t--) {                                                                                                                                    \
      lv_timer_handler();                                                                                                                            \
      delay(1);                                                                                                                                      \
    }                                                                                                                                                \
  } while (0);

void ui_begin();
void ui_switch_page();