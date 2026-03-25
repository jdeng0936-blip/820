#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <lvgl.h>
#include <lcd_port.h>
#include "lv_tc.h"
#include "at24cxx.h"
#define ERR_LIMIT					500

lv_indev_t * indev_touchpad;

extern void nvs_coeff_save_cb(lv_tc_coeff_t coeff);

static inline void bubble_sort(int16_t a[], int n)
{
	int i = 0,j = 0;
	int16_t temp = 0;
	for(i=0; i<n-1; i++)
	{
		int isSorted = 1;
		for(j = 0; j<n-1-i; j++)
		{
		    if(a[j] > a[j+1])
		    {
				isSorted = 0;
				temp = a[j];
				a[j] = a[j+1];
				a[j+1]=temp;
		    }
		}
		if(isSorted) 
			break; 
	}
}

static inline int tsc_filter(const int16_t *x, const int16_t *y, const int cread)
{
	int tmp = 0;
	int new_value, i = 0;
	int value = 0;
	for(i= 1; i < cread; i++) {
		value = x[i-1];
		new_value = x[i];
		tmp  = new_value - value;
	 	if (tmp > ERR_LIMIT)
	 		return 0;
	}
	
	tmp = 0;
	value = 0;
	new_value = 0;
 
	for(i= 1; i < cread; i++) {
		value = y[i-1];
		new_value = y[i];
	 	tmp  = new_value - value;
	 	if (tmp > ERR_LIMIT)
	 		return 0;
	}
	return 1;
}
lv_indev_state_t last_state = LV_INDEV_STATE_REL;
extern at24cxx_device_t i2c_dev;
volatile uint8_t touch_pressed = 0;
struct rt_i2c_msg msgs[2];
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
	rt_err_t result;
	uint16_t AddrBuf;
	static lv_coord_t last_x = 0;
	static lv_coord_t last_y = 0;
	uint8_t rxdata[2] = {0};
  int16_t touchX, touchY, touchZ;
	int16_t tX[5] = {0}, tY[5] = {0}, tZ[5] = {0};
	uint8_t timeout = 100;
	
_restart:
	for(int i=0;i<5;i++)
	{
		msgs[0].addr = 0x48;
		msgs[0].flags = RT_I2C_WR;
		AddrBuf = 0xE0;
		msgs[0].buf = (uint8_t *)&AddrBuf;
		msgs[0].len = 1;
		
		msgs[1].addr = 0x48;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = rxdata;
    msgs[1].len = 2;
		result = rt_mutex_take(i2c_dev->lock, RT_WAITING_FOREVER);
    if (result == RT_EOK)
		{
			rt_i2c_transfer(i2c_dev->i2c, msgs, 2);
		}
		rt_mutex_release(i2c_dev->lock);
		tZ[i] = (rxdata[0]<<4)|(rxdata[1]>>4);
		
		msgs[0].addr = 0x48;
		msgs[0].flags = RT_I2C_WR;
		AddrBuf = 0xC0;
		msgs[0].buf = (uint8_t *)&AddrBuf;
		msgs[0].len = 1;
		
		msgs[1].addr = 0x48;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = rxdata;
    msgs[1].len = 2;
		result = rt_mutex_take(i2c_dev->lock, RT_WAITING_FOREVER);
    if (result == RT_EOK)
		{
			rt_i2c_transfer(i2c_dev->i2c, msgs, 2);
		}
		rt_mutex_release(i2c_dev->lock);
		tX[i] = (rxdata[0]<<4)|(rxdata[1]>>4);
		
		msgs[0].addr = 0x48;
		msgs[0].flags = RT_I2C_WR;
		AddrBuf = 0xD0;
		msgs[0].buf = (uint8_t *)&AddrBuf;
		msgs[0].len = 1;
		
		msgs[1].addr = 0x48;
    msgs[1].flags = RT_I2C_RD;
    msgs[1].buf = rxdata;
    msgs[1].len = 2;
		result = rt_mutex_take(i2c_dev->lock, RT_WAITING_FOREVER);
    if (result == RT_EOK)
		{
			rt_i2c_transfer(i2c_dev->i2c, msgs, 2);
		}
		rt_mutex_release(i2c_dev->lock);
		tY[i] = (rxdata[0]<<4)|(rxdata[1]>>4);
	}
	bubble_sort(tX, 5);
	bubble_sort(tY, 5);
	bubble_sort(tZ, 5);
	
	if(!tsc_filter(tX, tY, 5))
	{
		timeout--;
		if(timeout)
			goto _restart;
	}
	
	touchX = (tX[1] + tX[2] + tX[3])/3;
	touchY = (tY[1] + tY[2] + tY[3])/3;
	touchZ = (tZ[1] + tZ[2] + tZ[3])/3;
	
	touchX = 800 - touchX * 800 / 4096;
  touchY = touchY * 480 / 4096;

  if (touchZ < 30) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;

    /*Set the coordinates*/
    data->point.x = touchX;
    data->point.y = touchY;
		last_x = touchX;
		last_y = touchY;
		
		//rt_kprintf("Touch: x=%d y=%d\r\n", touchX, touchY);
#if LV_USE_LOG != 0
    Serial.printf("Touch: x=%d y=%d\r\n", touchX, touchY);
#endif
  }
	
	data->point.x = last_x;
	data->point.y = last_y;
	
	if(last_state != data->state)
	{
		last_state = data->state;
		if(data->state == LV_INDEV_STATE_PR)
			touch_pressed = 1;
	}
}

void lv_port_indev_init(void)
{		
		static lv_indev_drv_t indev_drv;
		lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
		
		lv_tc_indev_drv_init(&indev_drv, touchpad_read);
		
    indev_touchpad = lv_indev_drv_register(&indev_drv);
		
		lv_tc_register_coeff_save_cb(nvs_coeff_save_cb);
}
