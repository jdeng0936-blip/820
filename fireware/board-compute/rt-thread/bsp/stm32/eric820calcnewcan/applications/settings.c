#include <rtthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#include "at24cxx.h"

extern at24cxx_device_t i2c_dev;
extern CALC_STATUS calc_status;

void def_settings(void)
{
	calc_status.settings.pt = 100;
	calc_status.settings.ct = 100;

	calc_status.settings.i0_limt = 20.0f;
	calc_status.settings.i0_ct = 100;
	
	calc_status.settings.v_over = 69;
	calc_status.settings.v_drop = 46;
	calc_status.settings.v_zero = 30;
	calc_status.settings.u0_limt = 65;
	
	calc_status.settings.rms_ratio_ua = 109.940002f;
	calc_status.settings.rms_ratio_ub = 108.919998f;
	calc_status.settings.rms_ratio_uc = 108.919998f;
	calc_status.settings.rms_ratio_u0 = 109.550003f;
	calc_status.settings.rms_ratio_ia = 2000.0f;
	calc_status.settings.rms_ratio_ib = 2000.0f;
	calc_status.settings.rms_ratio_ic = 2000.0f;
	calc_status.settings.rms_ratio_i0 = 4000.0f;
	calc_status.settings.sign = 0xA55A;
}
void save_settings(void)
{
	at24cxx_write(i2c_dev, 0x00, (uint8_t *)&calc_status.settings, sizeof(calc_status.settings));
}

void get_settings(void)
{
	at24cxx_read(i2c_dev, 0x00, (uint8_t *)&calc_status.settings, sizeof(calc_status.settings));
	if(calc_status.settings.sign != 0xA55A)
	{
		def_settings();
		save_settings();
	}
	sprintf(calc_status.settings.ver, CALC_VER);
}

