#ifndef WB_UI_TZ_H
#define WB_UI_TZ_H

#include <stddef.h>
#include <stdint.h>

#define UI_TZ_COUNT 7

void ui_tz_init(void);
void ui_tz_apply_env(uint8_t idx);
void ui_tz_set(uint8_t idx);
uint8_t ui_tz_get(void);
const char *ui_tz_name(uint8_t idx);
void ui_format_local_clock(char *out, size_t out_sz);

#endif
