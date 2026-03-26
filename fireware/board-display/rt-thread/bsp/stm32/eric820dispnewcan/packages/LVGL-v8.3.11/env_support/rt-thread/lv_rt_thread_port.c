/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: MIT
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-10-18     Meco Man     the first version
 * 2022-05-10     Meco Man     improve rt-thread initialization process
 */

#ifdef __RTTHREAD__

#include <lvgl.h>
#include <rtthread.h>
#include "ui.h"
#include <stdio.h>
#include <main.h>

#include <time.h>
#include <sys/time.h>

#define DBG_TAG    "LVGL"
#define DBG_LVL    DBG_INFO
#include <rtdbg.h>

#ifndef PKG_LVGL_THREAD_STACK_SIZE
#define PKG_LVGL_THREAD_STACK_SIZE 4096
#endif /* PKG_LVGL_THREAD_STACK_SIZE */

#ifndef PKG_LVGL_THREAD_PRIO
#define PKG_LVGL_THREAD_PRIO (RT_THREAD_PRIORITY_MAX*2/3)
#endif /* PKG_LVGL_THREAD_PRIO */

extern void lv_port_disp_init(void);
extern void lv_port_indev_init(void);
extern void lv_user_gui_init(void);
extern int8_t wavebuf[];
extern int8_t error_wavebuf[];
extern volatile uint8_t disp_wave_update;
extern volatile uint8_t disp_settings_update;
extern volatile uint8_t calc_status_update;
extern volatile uint8_t calc_status_ready;
static struct rt_thread lvgl_thread;
extern volatile uint8_t ui_loaded;
extern SETTINGS settings;
extern CALC_STATUS calc_status;
extern volatile uint8_t error_wave_update;
extern volatile uint8_t gnd_error_wave_upload;
extern volatile uint8_t voltage_error_wave_upload;
extern volatile uint8_t gnd_error_wave_save;
extern volatile uint8_t over_error_wave_save;
extern volatile uint8_t drop_error_wave_save;
extern volatile uint8_t pt_error_wave_save;
extern volatile uint8_t short_error_wave_save;
extern volatile uint8_t error_wave_update;
extern volatile uint8_t time_sync_req;
extern volatile uint8_t last_error_is_perm;
extern volatile uint8_t time_setting_changed;
extern volatile uint8_t time_settings_auto_update;
volatile uint8_t operation_running = 0;
volatile uint8_t line_error = 0;
volatile uint8_t line_lost = 0;
volatile uint8_t line_gnd = 0;
volatile uint8_t client_online_changed = 0;

extern lv_chart_series_t * ui_chartKV_series_1;
extern lv_chart_series_t * ui_chartKV_series_2;
extern lv_chart_series_t * ui_chartKV_series_3;

extern lv_chart_series_t * ui_chartKV2_series_1;
extern lv_chart_series_t * ui_chartKV2_series_2;
extern lv_chart_series_t * ui_chartKV2_series_3;

extern lv_chart_series_t * ui_chartKV4_series_1;
extern lv_chart_series_t * ui_chartKV4_series_2;
extern lv_chart_series_t * ui_chartKV4_series_3;
extern lv_chart_series_t * ui_chartKV4_series_4;

extern lv_chart_series_t * ui_chartKV5_series_1;
extern lv_chart_series_t * ui_chartKV5_series_2;
extern lv_chart_series_t * ui_chartKV5_series_3;
extern lv_chart_series_t * ui_chartKV5_series_4;

extern lv_obj_t * ui_chartKV;
extern lv_obj_t * ui_chartKV2;
extern lv_obj_t * ui_chartKV4;
extern lv_obj_t * ui_chartKV5;

extern volatile uint8_t display_switch_req;
extern int8_t error_bin[];
extern volatile uint8_t error_wave_hist_update;
extern volatile uint8_t disp_node_page_idx;
extern volatile uint8_t disp_node_page_cnt;
extern volatile uint8_t login;
volatile uint8_t disp_node_page_update = 0;


extern uint8_t gnd_error_idx;
extern uint8_t gnd_error_cnt;
extern uint8_t over_error_idx;
extern uint8_t over_error_cnt;
extern uint8_t drop_error_idx;
extern uint8_t drop_error_cnt;
extern uint8_t pt_error_idx;
extern uint8_t pt_error_cnt;
extern uint8_t short_error_idx;
extern uint8_t short_error_cnt;

lv_obj_t * nodelist_table[8][4];
extern uint16_t client_nodes_cnt;
extern NODE client_nodes[];
GND_ERROR_INFO gnd_error_info;
#ifdef rt_align
rt_align(RT_ALIGN_SIZE)
#else
ALIGN(RT_ALIGN_SIZE)
#endif
static rt_uint8_t lvgl_thread_stack[PKG_LVGL_THREAD_STACK_SIZE];

#if LV_USE_LOG
static void lv_rt_log(const char *buf)
{
    LOG_I(buf);
}
#endif /* LV_USE_LOG */

int16_t g_er_ua[384];
int16_t g_er_ub[384];
int16_t g_er_uc[384];
int16_t g_er_u0[384];
int16_t g_er_ie_1[384];
int16_t g_er_ie_2[384];
int16_t g_er_ie_3[384];
float g_er_rms_ua;
float g_er_rms_ub;
float g_er_rms_uc;
float g_er_rms_u0;
float g_er_rms[3];
uint16_t g_er_canid_1;
uint16_t g_er_canid_2;
uint16_t g_er_canid_3;

extern float rms_result_ua;
extern float rms_result_ub;
extern float rms_result_uc;
extern float rms_result_u0;
extern float rms_result_ia;
extern float rms_result_ib;
extern float rms_result_ic;
extern float rms_result_i0;

struct timeval tv_now = { 0 };
struct timeval tv_last = { 0 };

ERROR_INFO * error_info_disp;

volatile uint32_t idle_time = 0;
extern volatile uint8_t backlight_off;
extern volatile uint8_t backlight_on;
static void lvgl_thread_entry(void *parameter)
{
#if LV_USE_LOG
    lv_log_register_print_cb(lv_rt_log);
#endif /* LV_USE_LOG */
		char str[64];
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lv_user_gui_init();
	
		set_lcd_backlight_percent(settings.bl_percent);

    /* handle the tasks of LVGL */
    while(1)
    {
        lv_task_handler();
        rt_thread_mdelay(LV_DISP_DEF_REFR_PERIOD);
			  gettimeofday(&tv_now, RT_NULL);
				if((ui_loaded == 1)&&(tv_now.tv_sec != tv_last.tv_sec))
				{
					idle_time++;
					if(idle_time == settings.idle)
					{
						backlight_off = 1;
						lv_obj_clear_flag(ui_paddialog, LV_OBJ_FLAG_HIDDEN);
						lv_textarea_set_text(ui_pttextarea2, "");
						lv_tabview_set_act(ui_TabView1, 0, LV_ANIM_OFF);
					}
					tv_last.tv_sec = tv_now.tv_sec;
					tv_last.tv_usec = tv_now.tv_usec;
					time_t rawtime;
					time(&rawtime);
					struct tm *timeinfo;
					timeinfo = localtime(&rawtime);
					if(time_settings_auto_update == 1)
					{
						sprintf(str, "%d", timeinfo->tm_year + 1900);
						lv_textarea_set_text(ui_yeartextarea, str);
						sprintf(str, "%02d", timeinfo->tm_mon + 1);
						lv_textarea_set_text(ui_monthtextarea, str);
						sprintf(str, "%02d", timeinfo->tm_mday);
						lv_textarea_set_text(ui_daytextarea, str);
						sprintf(str, "%02d", timeinfo->tm_hour);
						lv_textarea_set_text(ui_hourtextarea, str);
						sprintf(str, "%02d", timeinfo->tm_min);
						lv_textarea_set_text(ui_mintextarea, str);
						sprintf(str, "%02d", timeinfo->tm_sec);
						lv_textarea_set_text(ui_secondtextarea, str);
					}
					
					sprintf(str, "%d/%d/%d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
					lv_label_set_text(ui_Label153, str);
					//time_sync_req = 1;
				}
				
				if((ui_loaded == 1)&&(disp_node_page_update == 1))
				{
					disp_node_page_update = 0;
					
					uint8_t online_changed = 0;
					uint8_t mask_bit = 0;
					uint8_t mask_idx = 0;
					for(int i=0;i<client_nodes_cnt;i++)
					{
						mask_idx = i / 32;
						mask_bit = i % 32;
						if(calc_status.clients_mask[mask_idx] & (1<<mask_bit))
						{
							if(client_nodes[i].status == 0)
								online_changed = 1;
							client_nodes[i].status = 1;
						}
						else
						{
							if(client_nodes[i].status == 1)
								online_changed = 1;
							client_nodes[i].status = 0;
						}
					}
					if(online_changed == 1)
						client_online_changed = 1;
					uint16_t client_idx = (disp_node_page_idx - 1) * 8;
					uint16_t client_cnt;
					if(client_nodes_cnt>client_idx)
						client_cnt = client_nodes_cnt - client_idx;
					else
						client_cnt = 0;
					
					if(client_cnt > 8)
						client_cnt = 8;
					for(int i=0;i<8;i++)
					{
						if(client_cnt >0)
						{
							sprintf(str, "%d", client_idx+1);
							lv_label_set_text(nodelist_table[i][0], str);
							sprintf(str, "%d", client_nodes[client_idx].id);
							lv_label_set_text(nodelist_table[i][1], str);
							if(client_nodes[client_idx].status == 1)
							{
								lv_label_set_text(nodelist_table[i][2], "在线");
								lv_obj_set_style_text_color(nodelist_table[i][2], lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
							}
							else
							{
								lv_label_set_text(nodelist_table[i][2], "离线");
								lv_obj_set_style_text_color(nodelist_table[i][2], lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
							}
							lv_label_set_text(nodelist_table[i][3], client_nodes[client_idx].pos);
							
							lv_obj_clear_flag(nodelist_table[i][0], LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(nodelist_table[i][1], LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(nodelist_table[i][2], LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(nodelist_table[i][3], LV_OBJ_FLAG_HIDDEN);
							
							client_cnt--;
							client_idx++;
						}
						else
						{
							lv_obj_add_flag(nodelist_table[i][0], LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(nodelist_table[i][1], LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(nodelist_table[i][2], LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(nodelist_table[i][3], LV_OBJ_FLAG_HIDDEN);
						}
					}
					sprintf(str, "%d/%d", disp_node_page_idx, disp_node_page_cnt);
					lv_label_set_text(ui_Label113, str);
				}
				if((ui_loaded == 1)&&(error_wave_update == 1)&&(calc_status_ready == 1))
				{
					error_wave_update = 0;
					calc_status_ready = 0;
					lv_coord_t max = 0, max2 = 0;
					lv_coord_t min = 0, min2 = 0;
					for(int i=0;i<384;i++)
					{
						ui_chartKV4_series_1->y_points[i] = error_wavebuf[i*7];
						ui_chartKV4_series_2->y_points[i] = error_wavebuf[i*7+1];
						ui_chartKV4_series_3->y_points[i] = error_wavebuf[i*7+2];
						ui_chartKV4_series_4->y_points[i] = error_wavebuf[i*7+3];
						
						ui_chartKV5_series_1->y_points[i] = error_wavebuf[i*7+4];
						ui_chartKV5_series_2->y_points[i] = error_wavebuf[i*7+5];
						ui_chartKV5_series_3->y_points[i] = error_wavebuf[i*7+6];
						
						if(min>ui_chartKV4_series_1->y_points[i])
							min = ui_chartKV4_series_1->y_points[i];
						if(min>ui_chartKV4_series_2->y_points[i])
							min = ui_chartKV4_series_2->y_points[i];
						if(min>ui_chartKV4_series_3->y_points[i])
							min = ui_chartKV4_series_3->y_points[i];
						if(min>ui_chartKV4_series_4->y_points[i])
							min = ui_chartKV4_series_4->y_points[i];
						
						if(max<ui_chartKV4_series_1->y_points[i])
							max = ui_chartKV4_series_1->y_points[i];
						if(max<ui_chartKV4_series_2->y_points[i])
							max = ui_chartKV4_series_2->y_points[i];
						if(max<ui_chartKV4_series_3->y_points[i])
							max = ui_chartKV4_series_3->y_points[i];
						if(max<ui_chartKV4_series_4->y_points[i])
							max = ui_chartKV4_series_4->y_points[i];
						
						if(min2>ui_chartKV5_series_1->y_points[i])
							min2 = ui_chartKV5_series_1->y_points[i];
						if(min2>ui_chartKV5_series_2->y_points[i])
							min2 = ui_chartKV5_series_2->y_points[i];
						if(min2>ui_chartKV5_series_3->y_points[i])
							min2 = ui_chartKV5_series_3->y_points[i];
						
						if(max2<ui_chartKV5_series_1->y_points[i])
							max2 = ui_chartKV5_series_1->y_points[i];
						if(max2<ui_chartKV5_series_2->y_points[i])
							max2 = ui_chartKV5_series_2->y_points[i];
						if(max2<ui_chartKV5_series_3->y_points[i])
							max2 = ui_chartKV5_series_3->y_points[i];
					}
					
					if((max<5)&&(min>-5))
					{
						lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
					}
					else if(max > -min)
					{
						lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, -(max*10)/9, (max*10)/9);
					}
					else
					{
						lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, (min*10)/9, -(min*10)/9);
					}
					
					if((max2<5)&&(min2>-5))
					{
						lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
					}
					else if(max2 > -min2)
					{
						lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, -(max2*10)/9, (max2*10)/9);
					}
					else
					{
						lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, (min2*10)/9, -(min2*10)/9);
					}
					
					struct tm *timeinfo;
					timeinfo = localtime(&calc_status.error_ts);
					sprintf(str, "故障时间:%d/%d/%d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
					lv_label_set_text(ui_Label177, str);
					
					sprintf(str, "Ua = %.2fKV", calc_status.error_ua*settings.pt/1000.0f);
					lv_label_set_text(ui_Label163, str);
					sprintf(str, "Ub = %.2fKV", calc_status.error_ub*settings.pt/1000.0f);
					lv_label_set_text(ui_Label167, str);
					sprintf(str, "Uc = %.2fKV", calc_status.error_uc*settings.pt/1000.0f);
					lv_label_set_text(ui_Label168, str);
					sprintf(str, "U0 = %.2fV", calc_status.error_u0);
					lv_label_set_text(ui_Label169, str);
					
					if(calc_status.error_slow == 0)
					{
						gnd_error_wave_upload = 1;
						gnd_error_wave_save = 1;
						gnd_error_cnt++;
						if(gnd_error_cnt > 100)
							gnd_error_cnt = 100;
						gnd_error_idx = 1;
						sprintf(str, "故障总数:%d",gnd_error_cnt);
						lv_label_set_text(ui_Label180, str);
						sprintf(str, "当前故障:%d",gnd_error_idx);
						lv_label_set_text(ui_Label87, str);
						
						line_gnd = 1;
						
						if(calc_status.error_line == 1)
						{
							//A
							if(calc_status.error_type == PERI_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nA相发生瞬时接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 0;
							}
							else if(calc_status.error_type == PERM_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nA相发生持续接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 1;
							}
						}
						if(calc_status.error_line == 2)
						{
							//B
							if(calc_status.error_type == PERI_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nB相发生瞬时接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 0;
							}
							else if(calc_status.error_type == PERM_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nB相发生持续接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 1;
							}
						}
						if(calc_status.error_line == 3)
						{
							//C
							if(calc_status.error_type == PERI_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nC相发生瞬时接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 0;
							}
							else if(calc_status.error_type == PERM_GND)
							{
								//perm gnd
								sprintf(str, "%d/%d/%d %02d:%02d:%02d\nC相发生持续接地故障 \n传感器ID %d \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, calc_status.error_id_1);
								last_error_is_perm = 1;
							}
							idle_time = 0;
						}
						lv_label_set_text(ui_Label39, str);
						
						sprintf(str, "ID%d:%.2fA", calc_status.error_id_1, calc_status.rms_1*settings.i0_ct);
						lv_label_set_text(ui_Label170, str);
						
						sprintf(str, "ID%d:%.2fA", calc_status.error_id_2, calc_status.rms_2*settings.i0_ct);
						lv_label_set_text(ui_Label172, str);
						
						sprintf(str, "ID%d:%.2fA", calc_status.error_id_3, calc_status.rms_3*settings.i0_ct);
						lv_label_set_text(ui_Label173, str);
						lv_obj_clear_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
						lv_obj_clear_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
						lv_obj_clear_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						
						lv_tabview_set_act(ui_TabView3, 1, LV_ANIM_OFF);
					}
					else if(calc_status.error_slow == VOLTAGE_OVER)
					{
						voltage_error_wave_upload = 1;
						over_error_wave_save = 1;
						over_error_cnt++;
						if(over_error_cnt > 100)
							over_error_cnt = 100;
						over_error_idx = 1;
						sprintf(str, "故障总数:%d",over_error_cnt);
						lv_label_set_text(ui_Label180, str);
						sprintf(str, "当前故障:%d",over_error_idx);
						lv_label_set_text(ui_Label87, str);
						lv_tabview_set_act(ui_TabView3, 0, LV_ANIM_OFF);
						lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						sprintf(str, "%d/%d/%d %02d:%02d:%02d\n发生过压故障 \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
						lv_label_set_text(ui_Label39, str);
					}
					else if(calc_status.error_slow == VOLTAGE_DROP)
					{
						voltage_error_wave_upload = 1;
						drop_error_wave_save = 1;
						drop_error_cnt++;
						if(drop_error_cnt > 100)
							drop_error_cnt = 100;
						drop_error_idx = 1;
						sprintf(str, "故障总数:%d",drop_error_cnt);
						lv_label_set_text(ui_Label180, str);
						sprintf(str, "当前故障:%d",drop_error_idx);
						lv_label_set_text(ui_Label87, str);
						lv_tabview_set_act(ui_TabView3, 2, LV_ANIM_OFF);
						lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						sprintf(str, "%d/%d/%d %02d:%02d:%02d\n发生低压故障 \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
						lv_label_set_text(ui_Label39, str);
					}
					else if(calc_status.error_slow == PT_ERROR)
					{
						voltage_error_wave_upload = 1;
						pt_error_wave_save = 1;
						pt_error_cnt++;
						if(pt_error_cnt > 100)
							pt_error_cnt = 100;
						pt_error_idx = 1;
						sprintf(str, "故障总数:%d",pt_error_cnt);
						lv_label_set_text(ui_Label180, str);
						sprintf(str, "当前故障:%d",pt_error_idx);
						lv_label_set_text(ui_Label87, str);
						lv_tabview_set_act(ui_TabView3, 3, LV_ANIM_OFF);
						lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						sprintf(str, "%d/%d/%d %02d:%02d:%02d\n发生PT故障 \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
						lv_label_set_text(ui_Label39, str);
					}
					else if(calc_status.error_slow == SHORT_ERROR)
					{
						voltage_error_wave_upload = 1;
						short_error_wave_save = 1;
						short_error_cnt++;
						if(short_error_cnt > 100)
							short_error_cnt = 100;
						short_error_idx = 1;
						sprintf(str, "故障总数:%d",short_error_cnt);
						lv_label_set_text(ui_Label180, str);
						sprintf(str, "当前故障:%d",short_error_idx);
						lv_label_set_text(ui_Label87, str);
						lv_tabview_set_act(ui_TabView3, 4, LV_ANIM_OFF);
						lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
						lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						sprintf(str, "%d/%d/%d %02d:%02d:%02d\n发生短路故障 \n详情请点击 ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
						lv_label_set_text(ui_Label39, str);
					}
					g_er_rms_ua = calc_status.error_ua;
					g_er_rms_ub = calc_status.error_ub;
					g_er_rms_uc = calc_status.error_uc;
					g_er_rms_u0 = calc_status.error_u0;
					g_er_rms[0] = calc_status.rms_1;
					g_er_rms[1] = calc_status.rms_2;
					g_er_rms[2] = calc_status.rms_3;
					g_er_canid_1 = calc_status.error_id_1;
					g_er_canid_2 = calc_status.error_id_2;
					g_er_canid_3 = calc_status.error_id_3;
					
					
					//lv_dropdown_set_selected(ui_Dropdown4, 0);
					
					lv_obj_clear_flag(ui_dialog, LV_OBJ_FLAG_HIDDEN);
					backlight_on = 1;
					line_error = 1;
				}
				if((ui_loaded == 1)&&(error_wave_hist_update == 1))
				{
					error_wave_hist_update = 0;
					error_info_disp = (ERROR_INFO *)&error_bin[2688];
					lv_coord_t max = 0, max2 = 0;
					lv_coord_t min = 0, min2 = 0;
					if(error_info_disp->sign != 0x5A)
					{
						for(int i=0;i<384;i++)
						{
							ui_chartKV4_series_1->y_points[i] = 0;
							ui_chartKV4_series_2->y_points[i] = 0;
							ui_chartKV4_series_3->y_points[i] = 0;
							ui_chartKV4_series_4->y_points[i] = 0;
							
							ui_chartKV5_series_1->y_points[i] = 0;
							ui_chartKV5_series_2->y_points[i] = 0;
							ui_chartKV5_series_3->y_points[i] = 0;
						}
						lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
						lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
						struct tm *timeinfo;
						timeinfo = localtime(&error_info_disp->error_ts);
						sprintf(str, "故障时间:--/--/-- --:--:--");
						lv_label_set_text(ui_Label177, str);
						
						sprintf(str, "Ua = 0.00KV");
						lv_label_set_text(ui_Label163, str);
						sprintf(str, "Ub = 0.00KV");
						lv_label_set_text(ui_Label167, str);
						sprintf(str, "Uc = 0.00KV");
						lv_label_set_text(ui_Label168, str);
						sprintf(str, "U0 = 0.00V");
						lv_label_set_text(ui_Label169, str);
						
						if(error_info_disp->witch == 0)
						{
							sprintf(str, "故障总数:%d",gnd_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",gnd_error_idx);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",gnd_error_idx+1, gnd_error_cnt);
							lv_label_set_text(ui_Label137, str);
							
							sprintf(str, "ID-:0.00A");
							lv_label_set_text(ui_Label170, str);
							
							sprintf(str, "ID-:0.00A");
							lv_label_set_text(ui_Label172, str);
							
							sprintf(str, "ID-:0.00A");
							lv_label_set_text(ui_Label173, str);
							lv_obj_clear_flag(ui_Label176, LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							
							lv_tabview_set_act(ui_TabView3, 1, LV_ANIM_OFF);
							
						}
						else if(error_info_disp->witch == 1)
						{
							sprintf(str, "故障总数:%d",over_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",over_error_idx);
							lv_label_set_text(ui_Label87, str);
							lv_tabview_set_act(ui_TabView3, 0, LV_ANIM_OFF);
							sprintf(str, "%d / %d",over_error_idx+1, over_error_cnt);
							lv_label_set_text(ui_Label159, str);
							
							lv_obj_add_flag(ui_Label176, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							lv_tabview_set_act(ui_TabView3, 0, LV_ANIM_OFF);
						}
						else if(error_info_disp->witch == 2)
						{
							sprintf(str, "故障总数:%d",drop_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",drop_error_idx);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",drop_error_idx+1, drop_error_cnt);
							lv_label_set_text(ui_Label143, str);
							
							lv_obj_add_flag(ui_Label176, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							lv_tabview_set_act(ui_TabView3, 2, LV_ANIM_OFF);
						}
						else if(error_info_disp->witch == 3)
						{
							sprintf(str, "故障总数:%d",pt_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",pt_error_idx);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",pt_error_idx+1, pt_error_cnt);
							lv_label_set_text(ui_Label145, str);
							
							lv_obj_add_flag(ui_Label176, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							lv_tabview_set_act(ui_TabView3, 3, LV_ANIM_OFF);
						}
						else if(error_info_disp->witch == 4)
						{
							sprintf(str, "故障总数:%d",short_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",short_error_idx);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",short_error_idx+1, short_error_cnt);
							lv_label_set_text(ui_Label148, str);
							
							lv_obj_add_flag(ui_Label176, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							lv_tabview_set_act(ui_TabView3, 4, LV_ANIM_OFF);
						}
					}
					else
					{
						for(int i=0;i<384;i++)
						{
							ui_chartKV4_series_1->y_points[i] = error_bin[i*7];
							ui_chartKV4_series_2->y_points[i] = error_bin[i*7+1];
							ui_chartKV4_series_3->y_points[i] = error_bin[i*7+2];
							ui_chartKV4_series_4->y_points[i] = error_bin[i*7+3];
							
							ui_chartKV5_series_1->y_points[i] = error_bin[i*7+4];
							ui_chartKV5_series_2->y_points[i] = error_bin[i*7+5];
							ui_chartKV5_series_3->y_points[i] = error_bin[i*7+6];
							
							if(min>ui_chartKV4_series_1->y_points[i])
								min = ui_chartKV4_series_1->y_points[i];
							if(min>ui_chartKV4_series_2->y_points[i])
								min = ui_chartKV4_series_2->y_points[i];
							if(min>ui_chartKV4_series_3->y_points[i])
								min = ui_chartKV4_series_3->y_points[i];
							if(min>ui_chartKV4_series_4->y_points[i])
								min = ui_chartKV4_series_4->y_points[i];
							
							if(max<ui_chartKV4_series_1->y_points[i])
								max = ui_chartKV4_series_1->y_points[i];
							if(max<ui_chartKV4_series_2->y_points[i])
								max = ui_chartKV4_series_2->y_points[i];
							if(max<ui_chartKV4_series_3->y_points[i])
								max = ui_chartKV4_series_3->y_points[i];
							if(max<ui_chartKV4_series_4->y_points[i])
								max = ui_chartKV4_series_4->y_points[i];
							
							if(min2>ui_chartKV5_series_1->y_points[i])
								min2 = ui_chartKV5_series_1->y_points[i];
							if(min2>ui_chartKV5_series_2->y_points[i])
								min2 = ui_chartKV5_series_2->y_points[i];
							if(min2>ui_chartKV5_series_3->y_points[i])
								min2 = ui_chartKV5_series_3->y_points[i];
							
							if(max2<ui_chartKV5_series_1->y_points[i])
								max2 = ui_chartKV5_series_1->y_points[i];
							if(max2<ui_chartKV5_series_2->y_points[i])
								max2 = ui_chartKV5_series_2->y_points[i];
							if(max2<ui_chartKV5_series_3->y_points[i])
								max2 = ui_chartKV5_series_3->y_points[i];
						}
						
						if((max<5)&&(min>-5))
						{
							lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
						}
						else if(max > -min)
						{
							lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, -(max*10)/9, (max*10)/9);
						}
						else
						{
							lv_chart_set_range(ui_chartKV4, LV_CHART_AXIS_PRIMARY_Y, (min*10)/9, -(min*10)/9);
						}
						
						if((max2<5)&&(min2>-5))
						{
							lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
						}
						else if(max2 > -min2)
						{
							lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, -(max2*10)/9, (max2*10)/9);
						}
						else
						{
							lv_chart_set_range(ui_chartKV5, LV_CHART_AXIS_PRIMARY_Y, (min2*10)/9, -(min2*10)/9);
						}
						
						struct tm *timeinfo;
						timeinfo = localtime(&error_info_disp->error_ts);
						sprintf(str, "故障时间:%d/%d/%d %02d:%02d:%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
						lv_label_set_text(ui_Label177, str);
						
						sprintf(str, "Ua = %.2fKV", error_info_disp->error_ua*settings.pt/1000.0f);
						lv_label_set_text(ui_Label163, str);
						sprintf(str, "Ub = %.2fKV", error_info_disp->error_ub*settings.pt/1000.0f);
						lv_label_set_text(ui_Label167, str);
						sprintf(str, "Uc = %.2fKV", error_info_disp->error_uc*settings.pt/1000.0f);
						lv_label_set_text(ui_Label168, str);
						sprintf(str, "U0 = %.2fV", error_info_disp->error_u0);
						lv_label_set_text(ui_Label169, str);
						
						if(error_info_disp->error_slow == 0)
						{
							sprintf(str, "故障总数:%d",gnd_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",gnd_error_idx+1);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",gnd_error_idx+1, gnd_error_cnt);
							lv_label_set_text(ui_Label137, str);
							
							sprintf(str, "ID%d:%.2fA", error_info_disp->error_id_1, error_info_disp->rms_1*settings.i0_ct);
							lv_label_set_text(ui_Label170, str);
							
							sprintf(str, "ID%d:%.2fA", error_info_disp->error_id_2, error_info_disp->rms_2*settings.i0_ct);
							lv_label_set_text(ui_Label172, str);
							
							sprintf(str, "ID%d:%.2fA", error_info_disp->error_id_3, error_info_disp->rms_3*settings.i0_ct);
							lv_label_set_text(ui_Label173, str);
							
							lv_obj_clear_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_clear_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
							
							lv_tabview_set_act(ui_TabView3, 1, LV_ANIM_OFF);
							
						}
						else if(error_info_disp->error_slow == VOLTAGE_OVER)
						{
							sprintf(str, "故障总数:%d",over_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",over_error_idx+1);
							lv_label_set_text(ui_Label87, str);
							
							sprintf(str, "%d / %d",over_error_idx+1, over_error_cnt);
							lv_label_set_text(ui_Label159, str);
							
							lv_tabview_set_act(ui_TabView3, 0, LV_ANIM_OFF);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						}
						else if(error_info_disp->error_slow == VOLTAGE_DROP)
						{
							sprintf(str, "故障总数:%d",drop_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",drop_error_idx+1);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",drop_error_idx+1, drop_error_cnt);
							lv_label_set_text(ui_Label143, str);
							
							lv_tabview_set_act(ui_TabView3, 2, LV_ANIM_OFF);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						}
						else if(error_info_disp->error_slow == PT_ERROR)
						{
							sprintf(str, "故障总数:%d",pt_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",pt_error_idx+1);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",pt_error_idx+1, pt_error_cnt);
							lv_label_set_text(ui_Label145, str);
							
							lv_tabview_set_act(ui_TabView3, 3, LV_ANIM_OFF);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						}
						else if(error_info_disp->error_slow == SHORT_ERROR)
						{
							sprintf(str, "故障总数:%d",short_error_cnt);
							lv_label_set_text(ui_Label180, str);
							sprintf(str, "当前故障:%d",short_error_idx+1);
							lv_label_set_text(ui_Label87, str);
							sprintf(str, "%d / %d",short_error_idx+1, short_error_cnt);
							lv_label_set_text(ui_Label148, str);
							
							lv_tabview_set_act(ui_TabView3, 4, LV_ANIM_OFF);
							lv_obj_add_flag(ui_Label170, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label172, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label173, LV_OBJ_FLAG_HIDDEN);
							lv_obj_add_flag(ui_Label174, LV_OBJ_FLAG_HIDDEN);
						}
					}
				}
				if((ui_loaded == 1)&&(disp_wave_update == 1))
				{
					disp_wave_update = 0;
					lv_coord_t max = 0;
					lv_coord_t min = 0;
					lv_coord_t max_c = 0;
					lv_coord_t min_c = 0;
					for(int i=0;i<384;i++)
					{
						ui_chartKV_series_1->y_points[i] = wavebuf[i*6];
						ui_chartKV_series_2->y_points[i] = wavebuf[i*6+1];
						ui_chartKV_series_3->y_points[i] = wavebuf[i*6+2];
						ui_chartKV2_series_1->y_points[i] = wavebuf[i*6+3];
						ui_chartKV2_series_2->y_points[i] = wavebuf[i*6+4];
						ui_chartKV2_series_3->y_points[i] = wavebuf[i*6+5];
						
						if(min>ui_chartKV_series_1->y_points[i])
							min = ui_chartKV_series_1->y_points[i];
						if(min>ui_chartKV_series_2->y_points[i])
							min = ui_chartKV_series_2->y_points[i];
						if(min>ui_chartKV_series_3->y_points[i])
							min = ui_chartKV_series_3->y_points[i];
						
						if(max<ui_chartKV_series_1->y_points[i])
							max = ui_chartKV_series_1->y_points[i];
						if(max<ui_chartKV_series_2->y_points[i])
							max = ui_chartKV_series_2->y_points[i];
						if(max<ui_chartKV_series_3->y_points[i])
							max = ui_chartKV_series_3->y_points[i];
						
						if(min_c>ui_chartKV2_series_1->y_points[i])
							min_c = ui_chartKV2_series_1->y_points[i];
						if(min_c>ui_chartKV2_series_2->y_points[i])
							min_c = ui_chartKV2_series_2->y_points[i];
						if(min_c>ui_chartKV2_series_3->y_points[i])
							min_c = ui_chartKV2_series_3->y_points[i];
						
						if(max_c<ui_chartKV2_series_1->y_points[i])
							max_c = ui_chartKV2_series_1->y_points[i];
						if(max_c<ui_chartKV2_series_2->y_points[i])
							max_c = ui_chartKV2_series_2->y_points[i];
						if(max_c<ui_chartKV2_series_3->y_points[i])
							max_c = ui_chartKV2_series_3->y_points[i];
					}
					
					if((max<5)&&(min>-5))
					{
						lv_chart_set_range(ui_chartKV, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
					}
					else if(max > -min)
					{
						lv_chart_set_range(ui_chartKV, LV_CHART_AXIS_PRIMARY_Y, -(max*10)/9, (max*10)/9);
					}
					else
					{
						lv_chart_set_range(ui_chartKV, LV_CHART_AXIS_PRIMARY_Y, (min*10)/9, -(min*10)/9);
					}
					
					if((max_c<5)&&(min_c>-5))
					{
						lv_chart_set_range(ui_chartKV2, LV_CHART_AXIS_PRIMARY_Y, -10, 10);
					}
					else if(max_c > -min_c)
					{
						lv_chart_set_range(ui_chartKV2, LV_CHART_AXIS_PRIMARY_Y, -(max_c*10)/9, (max_c*10)/9);
					}
					else
					{
						lv_chart_set_range(ui_chartKV2, LV_CHART_AXIS_PRIMARY_Y, (min_c*10)/9, -(min_c*10)/9);
					}
					
					sprintf(str, "Ua = %.2fKV", rms_result_ua);
					lv_label_set_text(ui_Label80, str);
					lv_label_set_text(ui_Label66, str);
					sprintf(str, "Ub = %.2fKV", rms_result_ub);
					lv_label_set_text(ui_Label81, str);
					lv_label_set_text(ui_Label68, str);
					sprintf(str, "Uc = %.2fKV", rms_result_uc);
					lv_label_set_text(ui_Label82, str);
					lv_label_set_text(ui_Label70, str);
					sprintf(str, "U0 = %.2fV", rms_result_u0);
					lv_label_set_text(ui_Label83, str);
					lv_label_set_text(ui_Label72, str);
					
					if((rms_result_ua<0.1f)&&(rms_result_ub<0.1f)&&(rms_result_uc<0.1f))
					{
						line_lost = 1;
					}
					else
					{
						line_lost = 0;
					}
					
					sprintf(str, "Ia = %.2fA", rms_result_ia);
					lv_label_set_text_fmt(ui_Label79, str);
					lv_label_set_text_fmt(ui_Label67, str);
					sprintf(str, "Ib = %.2fA", rms_result_ib);
					lv_label_set_text_fmt(ui_Label84, str);
					lv_label_set_text_fmt(ui_Label69, str);
					sprintf(str, "Ic = %.2fA", rms_result_ic);
					lv_label_set_text_fmt(ui_Label85, str);
					lv_label_set_text_fmt(ui_Label71, str);
					sprintf(str, "I0 = %.2fA", rms_result_i0);
					lv_label_set_text_fmt(ui_Label86, str);
					lv_label_set_text_fmt(ui_Label73, str);
				}
				if((ui_loaded == 1)&&(disp_settings_update == 1))
				{
					disp_settings_update = 0;
					//1
					lv_textarea_set_text(ui_passtextarea, settings.pass);
					//2
					sprintf(str, "%d", settings.bl_percent);
					lv_textarea_set_text(ui_backlighttextarea2, str);
					sprintf(str, "%d", settings.idle);
					lv_textarea_set_text(ui_passtextarea2, str);
					//3
					sprintf(str, "%d", settings.pt);
					lv_textarea_set_text(ui_pttextarea, str);
					sprintf(str, "%d", settings.ct);
					lv_textarea_set_text(ui_cttextarea, str);
					sprintf(str, "%.2f", settings.i0_limt);
					lv_textarea_set_text(ui_i0overtextarea, str);
					sprintf(str, "%d", settings.i0_ct);
					lv_textarea_set_text(ui_i0overtextarea2, str);
					//4
					sprintf(str, "%d", settings.v_over);
					lv_textarea_set_text(ui_abcovertextarea2, str);
					sprintf(str, "%d", settings.v_drop);
					lv_textarea_set_text(ui_abcovertextarea, str);
					sprintf(str, "%d", settings.u0_limt);
					lv_textarea_set_text(ui_abcdroptextarea3, str);
					//5
					sprintf(str, "%d", settings.rs485_id);
					lv_textarea_set_text(ui_TextArea7, str);
					
					lv_textarea_set_text(ui_TextArea10, settings.mac);
					lv_textarea_set_text(ui_TextArea11, settings.self_addr);
					lv_textarea_set_text(ui_TextArea12, settings.netmask);
					lv_textarea_set_text(ui_TextArea13, settings.gw);
					
					if(settings.use4g == 1)
					{
						_ui_state_modify(ui_Checkbox2, LV_STATE_CHECKED, _UI_MODIFY_STATE_REMOVE);
						_ui_state_modify(ui_Checkbox1, LV_STATE_CHECKED, _UI_MODIFY_STATE_ADD);
						_ui_flag_modify(ui_Panel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
						_ui_flag_modify(ui_key9, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
					}
					else
					{
						_ui_state_modify(ui_Checkbox1, LV_STATE_CHECKED, _UI_MODIFY_STATE_REMOVE);
						_ui_state_modify(ui_Checkbox2, LV_STATE_CHECKED, _UI_MODIFY_STATE_ADD);
						_ui_flag_modify(ui_Panel1, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
						_ui_flag_modify(ui_key9, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
					}
					
					if(settings.rs485_baud == 1200)
						lv_dropdown_set_selected(ui_Dropdown1, 0);
					else if(settings.rs485_baud == 2400)
						lv_dropdown_set_selected(ui_Dropdown1, 1);
					else if(settings.rs485_baud == 4800)
						lv_dropdown_set_selected(ui_Dropdown1, 2);
					else if(settings.rs485_baud == 9600)
						lv_dropdown_set_selected(ui_Dropdown1, 3);
					
					lv_textarea_set_text(ui_TextArea9, settings.server_addr);
					sprintf(str, "%d", settings.server_port);
					lv_textarea_set_text(ui_TextArea8, str);
				}
				if((ui_loaded == 1)&&(calc_status_update == 1))
				{
					calc_status_update = 0;
					
//					if(calc_status.work_status & 0x01)
//					{
//						line_error = 1;
//					}
//					else
//					{
//						line_error = 0;
//					}
					
					if(calc_status.in_status & 0x01)
					{
						operation_running = 1;
					}
					else
					{
						operation_running = 0;
					}
					if(calc_status.out_status&0x01)
					{
						lv_label_set_text(ui_Label47, "分闸");
						lv_obj_set_style_bg_color(ui_Button5, lv_color_hex(0xFF0000), LV_PART_MAIN);
					}
					else
					{
						lv_label_set_text(ui_Label47, "合闸");
						lv_obj_set_style_bg_color(ui_Button5, lv_color_hex(0x00FF00), LV_PART_MAIN);
					}
					
					if(calc_status.out_status&0x02)
					{
						lv_label_set_text(ui_Label51, "分闸");
						lv_obj_set_style_bg_color(ui_Button9, lv_color_hex(0xFF0000), LV_PART_MAIN);
					}
					else
					{
						lv_label_set_text(ui_Label51, "合闸");
						lv_obj_set_style_bg_color(ui_Button9, lv_color_hex(0x00FF00), LV_PART_MAIN);
					}
					
					if(calc_status.out_status&0x04)
					{
						lv_label_set_text(ui_Label50, "分闸");
						lv_obj_set_style_bg_color(ui_Button8, lv_color_hex(0xFF0000), LV_PART_MAIN);
					}
					else
					{
						lv_label_set_text(ui_Label50, "合闸");
						lv_obj_set_style_bg_color(ui_Button8, lv_color_hex(0x00FF00), LV_PART_MAIN);
					}
					
					if(calc_status.out_status&0x08)
					{
						lv_label_set_text(ui_Label49, "分闸");
						lv_obj_set_style_bg_color(ui_Button7, lv_color_hex(0xFF0000), LV_PART_MAIN);
					}
					else
					{
						lv_label_set_text(ui_Label49, "合闸");
						lv_obj_set_style_bg_color(ui_Button7, lv_color_hex(0x00FF00), LV_PART_MAIN);
					}
					
					if(calc_status.out_status&0x10)
					{
						lv_label_set_text(ui_Label48, "分闸");
						lv_obj_set_style_bg_color(ui_Button6, lv_color_hex(0xFF0000), LV_PART_MAIN);
					}
					else
					{
						lv_label_set_text(ui_Label48, "合闸");
						lv_obj_set_style_bg_color(ui_Button6, lv_color_hex(0x00FF00), LV_PART_MAIN);
					}
					
					lv_label_set_text(ui_Label97, calc_status.settings.ver);
					
//					sprintf(str, "%d", settings.pt);
//					lv_textarea_set_text(ui_pttextarea, str);
//					sprintf(str, "%d", settings.ct);
//					lv_textarea_set_text(ui_cttextarea, str);

//					sprintf(str, "%.2f", settings.i0_limt);
//					lv_textarea_set_text(ui_i0overtextarea, str);
//					sprintf(str, "%d", settings.i0_ct);
//					lv_textarea_set_text(ui_i0overtextarea2, str);
//					
//					sprintf(str, "%d", settings.v_over);
//					lv_textarea_set_text(ui_abcovertextarea2, str);
//					sprintf(str, "%d", settings.v_drop);
//					lv_textarea_set_text(ui_abcovertextarea, str);
//					sprintf(str, "%d", settings.u0_limt);
//					lv_textarea_set_text(ui_abcdroptextarea3, str);
				}
				if(display_switch_req == 1)
				{
					display_switch_req = 0;
					lv_tabview_set_act(ui_TabView1, 3, LV_ANIM_OFF);
				}
    }
}

static int lvgl_thread_init(void)
{
    rt_err_t err;

    err = rt_thread_init(&lvgl_thread, "LVGL", lvgl_thread_entry, RT_NULL,
           &lvgl_thread_stack[0], sizeof(lvgl_thread_stack), PKG_LVGL_THREAD_PRIO, 10);
    if(err != RT_EOK)
    {
        LOG_E("Failed to create LVGL thread");
        return -1;
    }
    rt_thread_startup(&lvgl_thread);

    return 0;
}
INIT_ENV_EXPORT(lvgl_thread_init);

#endif /*__RTTHREAD__*/
