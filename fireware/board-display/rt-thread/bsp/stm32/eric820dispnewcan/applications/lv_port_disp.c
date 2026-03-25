#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <lvgl.h>
#include <lcd_port.h>
#include "lv_tc.h"
#include "ui.h"
#include "lv_tc_screen.h"
#include "main.h"
#include <ctype.h>
#include "lcd_port.h"
//#include "lv_demo_benchmark.h"

//struct drv_lcd_device
//{
//    struct rt_device parent;

//    struct rt_device_graphic_info lcd_info;

//    struct rt_semaphore lcd_lock;

//    /* 0:front_buf is being used 1: back_buf is being used*/
//    rt_uint8_t cur_buf;
//    rt_uint8_t *front_buf;
//    rt_uint8_t *back_buf;
//};

extern volatile uint8_t ui_loaded;
extern struct drv_lcd_device _lcd;

#define MY_DISP_HOR_RES		800
#define MY_DISP_VER_RES		480

volatile uint8_t disable_refresh = 0;

extern void Dma2d_Fill_bmp(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
		//Dma2d_Fill_bmp(area->x1,area->y1,area->x2,area->y2,(uint16_t*)color_p);
		if(disable_refresh == 0)
			_lcd.parent.control(&_lcd.parent, RTGRAPHIC_CTRL_RECT_UPDATE, RT_NULL);
    lv_disp_flush_ready(disp_drv);
}
rt_uint8_t * buf_2_1;
rt_uint8_t * buf_2_2;


void lv_port_disp_init(void)
{
		static lv_disp_draw_buf_t draw_buf_dsc_2;
		buf_2_1 = _lcd.lcd_info.framebuffer;//rt_malloc_align(MY_DISP_HOR_RES * MY_DISP_VER_RES * 2, 4);
		//buf_2_2 = _lcd.back_buf;//rt_malloc_align(MY_DISP_HOR_RES * 240 * 2, 4);
		lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, NULL, MY_DISP_HOR_RES * MY_DISP_VER_RES);  //’‚¿Ô“™ÃÓ–¥∆¡ƒª≥ﬂ¥Á /*Initialize the display buffer*/
		
		static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/
	
		disp_drv.hor_res = MY_DISP_HOR_RES;
    disp_drv.ver_res = MY_DISP_VER_RES;
		disp_drv.flush_cb = disp_flush;
		disp_drv.direct_mode = 1;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc_2;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
}

void tc_finish_cb(lv_event_t *event)
{
/*
		Load the application
*/
	char str[64] = {0};
	ui_init(); /* Implement this */
	ui_loaded = 1;
	//set id	Label94
	udid[0] = HAL_GetUIDw0();
	udid[1] = HAL_GetUIDw1();
	udid[2] = HAL_GetUIDw2();
	sprintf(str, "ERIC1820%08x", udid[2]^udid[1]^udid[0]);
	for(int i=0;i<strlen(str);i++)
	{
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] = toupper(str[i]);
	}
	
	lv_label_set_text(ui_Label94, str);
	//set calc ver Label97
	//lv_label_set_text(ui_Label97, CALC_VER);
	//set disp ver Label96
	lv_label_set_text(ui_Label96, DISP_VER);
}
extern uint8_t nvs_init_and_check(void);
void lv_user_gui_init(void)
{
	if(nvs_init_and_check())
	{
		tc_finish_cb(RT_NULL);
	}
	else
	{
		lv_obj_t *tCScreen = lv_tc_screen_create();
		lv_obj_add_event_cb(tCScreen, tc_finish_cb, LV_EVENT_READY, NULL);
		
		lv_disp_load_scr(tCScreen);
		lv_tc_screen_start(tCScreen);
	}
}
