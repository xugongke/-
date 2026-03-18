#ifndef __ILI9488_H
#define __ILI9488_H

#include "stm32f4xx_hal.h"
#include "gui_guider.h"
extern uint8_t s_last_user_no;

void user_list_item_event_handler(lv_event_t *e);
void ui_load_user_detail(lv_ui *ui, uint32_t user_no);


void sdcard_write(uint32_t i);
void Create_der(uint32_t i);
void Write_temperature(uint32_t i);
void Write_power(uint32_t i);
void Write_yongpower(uint32_t i);

#endif

