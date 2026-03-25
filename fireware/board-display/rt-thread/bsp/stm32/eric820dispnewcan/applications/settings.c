#include <rtthread.h>
#include <rthw.h>
#include "sfdb.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dfs.h>
#include <dfs_fs.h>

#include "main.h"
#include "lv_tc.h"
#include "at24cxx.h"

extern at24cxx_device_t i2c_dev;
SETTINGS settings;
extern CALC_STATUS calc_status;

void def_settings(void)
{
	//1
	sprintf(settings.pass, "1234");
	//2
	settings.bl_percent = 50;
	settings.idle = 300;
	//3
	settings.pt = 100;
	settings.ct = 100;
	settings.i0_limt = 20.00f;
	settings.i0_ct = 100;
	//4
	settings.v_over = 69;
	settings.v_drop = 46;
	settings.u0_limt = 65;
	//5
	settings.rs485_id = 1;
	settings.rs485_baud = 1200;
	sprintf(settings.server_addr, "43.137.46.155");
	settings.server_port = 8820;
	sprintf(settings.mac, "001234567899");
	sprintf(settings.self_addr, "192.168.3.239");
	sprintf(settings.netmask, "255.255.255.0");
	sprintf(settings.gw, "192.168.3.1");
	settings.use4g = 1;
	settings.sign = 0xA55A;
	//
}

void set_settings(void)
{
	at24cxx_write(i2c_dev, 0x00, (uint8_t *)&settings, sizeof(SETTINGS));
}

void get_settings(void)
{
	at24cxx_read(i2c_dev, 0x00, (uint8_t *)&settings, sizeof(SETTINGS));
	if(settings.sign != 0xA55A)//||(strcmp(settings.self_addr,"192.168.20.240")!=0))
	{
		def_settings();
		set_settings();
	}
	memset(calc_status.settings.ver, 0x00, sizeof(calc_status.settings.ver));
}

void save_client_cnt(uint16_t cnt)
{
	at24cxx_write(i2c_dev, 256, (uint8_t *)&cnt, 2);
}
uint16_t load_client_cnt(void)
{
	uint16_t cnt = 0;
	at24cxx_read(i2c_dev, 256, (uint8_t *)&cnt, 2);
	return cnt;
}


void nvs_coeff_save_cb(lv_tc_coeff_t coeff)
{
	at24cxx_write(i2c_dev, 128*8, (uint8_t *)&coeff, sizeof(lv_tc_coeff_t));
}

uint8_t nvs_init_and_check(void)
{
	lv_tc_coeff_t coeff;
	
	while(i2c_check_ok == 0)
		rt_thread_mdelay(1);
	at24cxx_read(i2c_dev, 128*8, (uint8_t *)&coeff, sizeof(lv_tc_coeff_t));
	
	if(coeff.isValid == true)
	{
		lv_tc_set_coeff(coeff, false);
		return 1;
	}
	else
	{
		return 0;
	}
}

void nvs_clear_coeff(void)
{
	lv_tc_coeff_t coeff;
	at24cxx_read(i2c_dev, 128*8, (uint8_t *)&coeff, sizeof(lv_tc_coeff_t));
	coeff.isValid = false;
	at24cxx_write(i2c_dev, 128*8, (uint8_t *)&coeff, sizeof(lv_tc_coeff_t));
	rt_hw_cpu_reset();
}
