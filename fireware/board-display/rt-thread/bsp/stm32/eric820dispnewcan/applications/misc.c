#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <lvgl.h>
#include <lcd_port.h>
#include <dfs_posix.h>
#include <mbedtls/md5.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <webclient.h>

#include "main.h"
#include "at24cxx.h"
#include "sfdb.h"
#include <stdlib.h>

#define UPDATE_CMD		0x982055AA
#define GND_SDB_FILE_PATH  		"/gnd_error.sdb"
#define OVER_SDB_FILE_PATH  	"/over_error.sdb"
#define DROP_SDB_FILE_PATH  	"/drop_error.sdb"
#define PT_SDB_FILE_PATH  		"/pt_error.sdb"
#define SHORT_SDB_FILE_PATH  	"/short_error.sdb"
#define NODE_SDB_FILE_PATH  	"/nodes.sdb"
#define NODE_MAX_RECORD_NUM  	1000
#define MAX_RECORD_NUM  			100
#define RECORD_LEN      			4096

#define MISC_THREAD_STACK_SIZE 512
#define MISC_THREAD_PRIO 29
static rt_uint8_t * misc_thread_stack;
static struct rt_thread misc_thread;
extern volatile uint8_t display_data_disable;
#define LAZY_THREAD_STACK_SIZE 8192
#define LAZY_THREAD_PRIO 28
static rt_uint8_t * lazy_thread_stack;
static struct rt_thread lazy_thread;
extern volatile uint8_t touch_pressed;
extern volatile uint8_t update_start;
extern volatile uint8_t download_file;
extern volatile uint8_t download_cancfg;
extern volatile uint8_t gnd_error_wave_save;
extern volatile uint8_t over_error_wave_save;
extern volatile uint8_t drop_error_wave_save;
extern volatile uint8_t pt_error_wave_save;
extern volatile uint8_t short_error_wave_save;
extern volatile uint8_t error_wave_update;
extern volatile uint8_t mqtt_start_req;
extern RTC_HandleTypeDef RTC_Handler;
extern struct rt_memheap system_heap;
extern CALC_STATUS calc_status;
extern int8_t error_wavebuf[];
extern volatile uint8_t client_node_load;
extern volatile uint8_t save_settings_req;
extern volatile uint8_t save_request_reset;
volatile uint32_t hum_det_cnt = 0;
uint8_t gnd_error_idx = 0;
uint8_t gnd_error_cnt = 0;
uint8_t over_error_idx = 0;
uint8_t over_error_cnt = 0;
uint8_t drop_error_idx = 0;
uint8_t drop_error_cnt = 0;
uint8_t pt_error_idx = 0;
uint8_t pt_error_cnt = 0;
uint8_t short_error_idx = 0;
uint8_t short_error_cnt = 0;
extern volatile uint8_t gnd_error_wave_load;
extern volatile uint8_t over_error_wave_load;
extern volatile uint8_t drop_error_wave_load;
extern volatile uint8_t pt_error_wave_load;
extern volatile uint8_t short_error_wave_load;

volatile uint8_t error_wave_hist_update = 0;
extern volatile uint8_t ui_loaded;
extern NODE client_nodes[];
volatile uint8_t settings_loaded = 0;
uint16_t client_nodes_cnt = 0;
extern void set_settings(void);
uint8_t voice_data[32];
uint8_t voice_len;
volatile uint8_t backlight_on = 0;
volatile uint8_t backlight_off = 0;
extern void turn_on_lcd_backlight(void);
extern void turn_off_lcd_backlight(void);
//volatile uint8_t backlight_status = 1;
extern volatile uint32_t idle_time;
extern volatile uint8_t disp_node_page_update;
extern volatile uint8_t login;
volatile uint32_t dwt_fac_us;
volatile uint32_t dwt_fac_ms;
extern char update_url[];
volatile uint8_t sync_update_file = 0;
volatile uint8_t sync_cancfg_file = 0;
extern volatile uint8_t disp_node_page_idx;
extern volatile uint8_t disp_node_page_cnt;
extern at24cxx_device_t i2c_dev;
volatile uint8_t save_nodelist_req = 0;
extern char fmd51[];
extern char fmd52[];

//void set_lcd_backlight_percent(uint8_t percent)
//{
//	uint32_t peri = 45u * percent;
//	if(peri >4500)
//		peri = 4499;
//	struct rt_device_pwm *pwm_dev;
//	pwm_dev = (struct rt_device_pwm *)rt_device_find(LCD_PWM_DEV_NAME);
//	rt_pwm_set(pwm_dev, LCD_PWM_DEV_CHANNEL, 4500, peri);
//}

//void turn_off_lcd_backlight(void)
//{
//    struct rt_device_pwm *pwm_dev;

//    /* turn on the LCD backlight */
//    pwm_dev = (struct rt_device_pwm *)rt_device_find(LCD_PWM_DEV_NAME);
//    /* pwm frequency:100K = 10000ns */
//    //rt_pwm_set(pwm_dev, LCD_PWM_DEV_CHANNEL, 4500, 2250);
//    rt_pwm_disable(pwm_dev, LCD_PWM_DEV_CHANNEL);
//		backlight_status = 0;
//}
/**
  * @brief  initialize dwt delay function   
  * @param  none
  * @retval none
  */
void dwt_delay_init()
{
	/* configure dwt */	
  dwt_fac_us = SystemCoreClock / (1000000U);
  dwt_fac_ms = dwt_fac_us * (1000U);	
}

/**
  * @brief  inserts a dwt delay time.
  * @param  nus: specifies the dwt delay time length, in microsecond.
  * @retval none
  */
void dwt_delay_us(uint32_t nus)
{
	/* Enable CoreDebug DEMCR bit24: TRCENA */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
 	/* Clear CYCCNT */
	DWT->CYCCNT = 0x00;
 	/* Enable DWT CTRL bit0: CYCCNTENA */	
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	
	uint32_t temp = DWT->CYCCNT;
  while((DWT->CYCCNT - temp) < (nus * dwt_fac_us - 40));

 	/* Disable DWT CTRL bit0: CYCCNTENA */	
	DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;
 	/* Clear CYCCNT */
	DWT->CYCCNT = 0x00;
	/* Disable CoreDebug DEMCR bit24: TRCENA */
	CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
}

void hum_det_irq(void* param)
{
	if(rt_pin_read(GET_PIN(H, 4)) == PIN_HIGH)
		hum_det_cnt++;
}

ERROR_INFO * error_info_misc;
uint8_t record[4096] = {0};
int8_t error_bin[4096] = {0};

void error_save(const char * file_path, uint8_t * idx, uint8_t * cnt) {
    sfdb_t sfdb;
    sfdb_cfg_t cfg = {0};
		uint32_t tick_old, duration;

    cfg.path = file_path;
    cfg.flags = SFDB_SYNC | SFDB_OVERWRITE;
    cfg.record_len = RECORD_LEN;
    cfg.max_record_num = MAX_RECORD_NUM;

    if (sfdb_open(&sfdb, &cfg) < 0) {
        return;
    }
		tick_old = rt_tick_get();
		if (sfdb_append(&sfdb, record, sizeof(record)) < 0) {
				rt_kprintf("append record failed.");
				sfdb_close(&sfdb);
				return;
		}
		duration = rt_tick_get() - tick_old;
		rt_kprintf("append 1 data cost %4d ms", duration);
    sfdb_close(&sfdb);
		*idx = 0;
		*cnt = sfdb.hdr.record_count;
}

void error_read(const char * file_path, uint8_t idx, uint8_t * cnt)
{
	sfdb_t sfdb;
	sfdb_cfg_t cfg = {0};
	uint32_t tick_old, duration;

	cfg.path = file_path;
	cfg.record_len = RECORD_LEN;
	cfg.max_record_num = MAX_RECORD_NUM;

	if (sfdb_open(&sfdb, &cfg) < 0) {
			return;
	}
	
	*cnt = sfdb.hdr.record_count;
	tick_old = rt_tick_get();
	error_info_misc = (ERROR_INFO *)&error_bin[2688];
	if(idx < sfdb.hdr.record_count)
	{
		sfdb_read(&sfdb, (uint8_t *)error_bin, RECORD_LEN, idx, 1, SFDB_READ_DESC);
		error_info_misc->sign = 0x5A;
	}
	else
	{
		error_info_misc->sign = 0x00;
	}
	error_wave_hist_update = 1;
	duration = rt_tick_get() - tick_old;
	rt_kprintf("read 1 data cost %4d ms", duration);
	sfdb_close(&sfdb);
}

void client_nodes_read(void)
{
	sfdb_t sfdb;
	sfdb_cfg_t cfg = {0};
	uint16_t page_cnt = 0;
	uint32_t tick_old, duration;

	cfg.path = NODE_SDB_FILE_PATH;
	cfg.record_len = RECORD_LEN;
	cfg.max_record_num = NODE_MAX_RECORD_NUM;

	if (sfdb_open(&sfdb, &cfg) < 0) {
			return;
	}
	
	client_nodes_cnt = load_client_cnt();
	if(client_nodes_cnt>999)
		client_nodes_cnt = 0;
	page_cnt = (client_nodes_cnt+127)/128;
	
	tick_old = rt_tick_get();
	if(client_nodes_cnt > 0)
		sfdb_read(&sfdb, (uint8_t *)client_nodes, RECORD_LEN*page_cnt, 0, page_cnt, SFDB_READ_ASC);
	duration = rt_tick_get() - tick_old;
	rt_kprintf("read %d data cost %4d ms", page_cnt, duration);
	sfdb_close(&sfdb);
	
	disp_node_page_idx = 1;
	disp_node_page_cnt = (client_nodes_cnt+7)/8;
	disp_node_page_update = 1;
}
void client_nodes_write(void)
{
	unlink(NODE_SDB_FILE_PATH);
	sfdb_t sfdb;
	sfdb_cfg_t cfg = {0};
	uint16_t page_cnt = 0;
	uint32_t tick_old, duration;
	uint8_t * pbuf = (uint8_t*)client_nodes;

	cfg.path = NODE_SDB_FILE_PATH;
	cfg.flags = SFDB_SYNC | SFDB_O_CREATE;
	cfg.record_len = RECORD_LEN;
	cfg.max_record_num = NODE_MAX_RECORD_NUM;

	if (sfdb_open(&sfdb, &cfg) < 0) {
			return;
	}
	page_cnt = (client_nodes_cnt+127)/128;
	
	for(int i=0;i<page_cnt;i++)
	{
		tick_old = rt_tick_get();
		if (sfdb_append(&sfdb, pbuf, sizeof(record)) < 0) {
				rt_kprintf("append record failed.");
				sfdb_close(&sfdb);
				return;
		}
		pbuf+=RECORD_LEN;
		duration = rt_tick_get() - tick_old;
		rt_kprintf("append 1 data cost %4d ms", duration);
	}
	sfdb_close(&sfdb);
	save_client_cnt(client_nodes_cnt);
	download_cancfg = 1;
}

void voice_send(uint8_t idx)
{
	rt_base_t level = rt_hw_interrupt_disable();
	rt_pin_write(GET_PIN(G, 14), PIN_HIGH);
	dwt_delay_us(80);
	rt_pin_write(GET_PIN(G, 14), PIN_LOW);
	dwt_delay_us(80);
	rt_pin_write(GET_PIN(G, 14), PIN_HIGH);
	dwt_delay_us(80);
	for(int i =0;i<idx;i++)
	{
		rt_pin_write(GET_PIN(C, 7), PIN_HIGH);
		dwt_delay_us(80);
		rt_pin_write(GET_PIN(C, 7), PIN_LOW);
		dwt_delay_us(80);
	}
	rt_pin_write(GET_PIN(C, 7), PIN_HIGH);
	dwt_delay_us(80);
	rt_hw_interrupt_enable(level);
}

static void misc_thread_entry(void *parameter)
{
		dwt_delay_init();
	
		rt_pin_mode(88, PIN_MODE_OUTPUT);
		rt_pin_write(88, PIN_HIGH);
	
		rt_pin_mode(GET_PIN(H, 4), PIN_MODE_INPUT);
		rt_pin_attach_irq(GET_PIN(H, 4), PIN_IRQ_MODE_RISING_FALLING, hum_det_irq, RT_NULL);
		rt_pin_irq_enable(GET_PIN(H, 4), PIN_IRQ_ENABLE);
		
		rt_pin_write(GET_PIN(C, 3), PIN_LOW);
		rt_pin_mode(GET_PIN(C, 3), PIN_MODE_OUTPUT);
	
		rt_pin_mode(GET_PIN(G, 13), PIN_MODE_INPUT);	//BUSY
		rt_pin_write(GET_PIN(C, 7), PIN_HIGH);
		rt_pin_mode(GET_PIN(C, 7), PIN_MODE_OUTPUT);	//P
		rt_pin_write(GET_PIN(G, 14), PIN_HIGH);
		rt_pin_mode(GET_PIN(G, 14), PIN_MODE_OUTPUT);	//REP
		//voice_send(43);
    while(1)
    {
      rt_thread_mdelay(10);
			if(touch_pressed == 1)
			{
				touch_pressed = 0;
				rt_pin_write(GET_PIN(C, 3), PIN_HIGH);
				rt_thread_mdelay(50);
				rt_pin_write(GET_PIN(C, 3), PIN_LOW);
				rt_thread_mdelay(100);
				idle_time = 0;
				if(backlight_status == 0)
				{
					backlight_on = 1;
				}
			}
			if(hum_det_cnt != 0)
			{
				hum_det_cnt = 0;
				idle_time = 0;
				//voice
				if(backlight_status == 0)
				{
					backlight_on = 1;
					if(rt_pin_read(GET_PIN(G, 13)) == PIN_LOW)
					{
						voice_send(3);
					}
				}
			}
			if(rt_pin_read(GET_PIN(H, 4)) == PIN_HIGH)
			{
				idle_time = 0;
				if(backlight_status == 0)
				{
					backlight_on = 1;
				}
			}
			if(backlight_on == 1)
			{
				backlight_on = 0;
				idle_time = 0;
				turn_on_lcd_backlight();
				//backlight_status = 1;
			}
			if(backlight_off == 1)
			{
				backlight_off = 0;
				turn_off_lcd_backlight();
			}
    }
}
extern uint16_t packet_cnt;
extern uint16_t packet_idx;
FILE * firmwaref;
extern uint8_t file_buf[];
volatile uint8_t display_switch_req = 0;
#define MD5_BUF_SIZE 1024
int md5_file(const char *filename, unsigned char *md5sum)
{
    int ret = -1;
    mbedtls_md5_context ctx;
    unsigned char buf[MD5_BUF_SIZE];
    int n;

    mbedtls_md5_init(&ctx);

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        rt_kprintf("Failed to open file %s\n", filename);
        goto exit;
    }

    mbedtls_md5_starts(&ctx);

    while ((n = fread(buf, 1, MD5_BUF_SIZE, fp)) > 0)
    {
        mbedtls_md5_update(&ctx, buf, n);
    }

    mbedtls_md5_finish(&ctx, md5sum);

    ret = 0;

exit:
    if (fp != NULL)
    {
        fclose(fp);
    }
    mbedtls_md5_free(&ctx);

    return ret;
}
extern uint16_t packet_len;
unsigned char md5sum2[16];
extern volatile uint8_t set_file_md5_req;
static void lazy_thread_entry(void *parameter)
{
	char uri[256];
	while(1)
	{
		rt_thread_mdelay(10);
		if(update_start == 1)
		{
			update_start = 0;
			HAL_RTCEx_BKUPWrite(&RTC_Handler, RTC_BKP_DR2, UPDATE_CMD);
			rt_thread_mdelay(1000);
			rt_hw_cpu_reset();
		}
		if(download_file == 1)
		{
			display_switch_req = 1;
			display_data_disable = 1;
//download_file1:
			memset(uri, 0x00, sizeof(uri));
			sprintf(uri, "%s.bin", update_url);
			webclient_get_file(uri, "rtthread.bin");
			unsigned char md5sum1[16];
			md5_file("/rtthread.bin", md5sum1);
			char f1[33] = {0};
			sprintf(f1, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", md5sum1[0], md5sum1[1], md5sum1[2], md5sum1[3], md5sum1[4], md5sum1[5], md5sum1[6], md5sum1[7], md5sum1[8]
			, md5sum1[9], md5sum1[10], md5sum1[11], md5sum1[12], md5sum1[13], md5sum1[14], md5sum1[15]);
			if(0 != strcasecmp(f1, fmd51))
			{
				rt_kprintf("%s\n",f1);
				rt_kprintf("%s\n",fmd51);
				//goto download_file1;
				download_file = 0;
				continue;
			}
//download_file2:
			memset(uri, 0x00, sizeof(uri));
			sprintf(uri, "%s_cal.bin", update_url);
			webclient_get_file(uri, "rtthread_cal.bin");
			md5_file("/rtthread_cal.bin", md5sum2);
			char f2[33] = {0};
			sprintf(f2, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", md5sum2[0], md5sum2[1], md5sum2[2], md5sum2[3], md5sum2[4], md5sum2[5], md5sum2[6], md5sum2[7], md5sum2[8]
			, md5sum2[9], md5sum2[10], md5sum2[11], md5sum2[12], md5sum2[13], md5sum2[14], md5sum2[15]);
			if(0 != strcasecmp(f2, fmd52))
			{
				rt_kprintf("%s\n",f2);
				rt_kprintf("%s\n",fmd52);
				//goto download_file2;
				download_file = 0;
				continue;
			}
			set_file_md5_req = 1;
			struct stat filestat;
			stat("rtthread_cal.bin", &filestat);
			packet_cnt = (filestat.st_size+4095) / 4096;
			firmwaref = fopen("rtthread_cal.bin", "rb");
			if(firmwaref != NULL)
			{
				packet_len = fread(file_buf, 1, 4096, firmwaref);
				packet_idx = 0;
				sync_update_file = 1;
			}
			download_file = 0;
		}
		if(download_cancfg == 1)
		{
			struct stat filestat;
			stat("cancfg.msg", &filestat);
			packet_cnt = (filestat.st_size+4095) / 4096;
			firmwaref = fopen("cancfg.msg", "rb");
			if(firmwaref != NULL)
			{
				packet_len = fread(file_buf, 1, 4096, firmwaref);
				packet_idx = 0;
				sync_cancfg_file = 1;
			}
			download_cancfg = 0;
		}
		if((ui_loaded == 1)&&(gnd_error_wave_save == 1))
		{
			gnd_error_wave_save = 0;
			memcpy(record, error_wavebuf, 2688);
			error_info_misc = (ERROR_INFO *)&record[2688];
			error_info_misc->rms_1 = calc_status.rms_1;
			error_info_misc->rms_2 = calc_status.rms_2;
			error_info_misc->rms_3 = calc_status.rms_3;
			error_info_misc->error_id_1 = calc_status.error_id_1;
			error_info_misc->error_id_2 = calc_status.error_id_2;
			error_info_misc->error_id_3 = calc_status.error_id_3;
			error_info_misc->error_line = calc_status.error_line;
			error_info_misc->error_slow = calc_status.error_slow;
			error_info_misc->error_ts = calc_status.error_ts;
			error_info_misc->error_type = calc_status.error_type;
			error_info_misc->error_ua = calc_status.error_ua;
			error_info_misc->error_ub = calc_status.error_ub;
			error_info_misc->error_uc = calc_status.error_uc;
			error_info_misc->error_u0 = calc_status.error_u0;
			error_save(GND_SDB_FILE_PATH, &gnd_error_idx, &gnd_error_cnt);
		}
		if((ui_loaded == 1)&&(over_error_wave_save == 1))
		{
			over_error_wave_save = 0;
			memcpy(record, error_wavebuf, 2688);
			error_info_misc = (ERROR_INFO *)&record[2688];
			error_info_misc->error_slow = calc_status.error_slow;
			error_info_misc->error_ts = calc_status.error_ts;
			error_info_misc->error_ua = calc_status.error_ua;
			error_info_misc->error_ub = calc_status.error_ub;
			error_info_misc->error_uc = calc_status.error_uc;
			error_info_misc->error_u0 = calc_status.error_u0;
			error_save(OVER_SDB_FILE_PATH, &over_error_idx, &over_error_cnt);
		}
		if((ui_loaded == 1)&&(drop_error_wave_save == 1))
		{
			drop_error_wave_save = 0;
			memcpy(record, error_wavebuf, 2688);
			error_info_misc = (ERROR_INFO *)&record[2688];
			error_info_misc->error_slow = calc_status.error_slow;
			error_info_misc->error_ts = calc_status.error_ts;
			error_info_misc->error_ua = calc_status.error_ua;
			error_info_misc->error_ub = calc_status.error_ub;
			error_info_misc->error_uc = calc_status.error_uc;
			error_info_misc->error_u0 = calc_status.error_u0;
			error_save(DROP_SDB_FILE_PATH, &drop_error_idx, &drop_error_cnt);
		}
		if((ui_loaded == 1)&&(pt_error_wave_save == 1))
		{
			pt_error_wave_save = 0;
			memcpy(record, error_wavebuf, 2688);
			error_info_misc = (ERROR_INFO *)&record[2688];
			error_info_misc->error_slow = calc_status.error_slow;
			error_info_misc->error_ts = calc_status.error_ts;
			error_info_misc->error_ua = calc_status.error_ua;
			error_info_misc->error_ub = calc_status.error_ub;
			error_info_misc->error_uc = calc_status.error_uc;
			error_info_misc->error_u0 = calc_status.error_u0;
			error_save(PT_SDB_FILE_PATH, &pt_error_idx, &pt_error_cnt);
		}
		if((ui_loaded == 1)&&(short_error_wave_save == 1))
		{
			short_error_wave_save = 0;
			memcpy(record, error_wavebuf, 2688);
			error_info_misc = (ERROR_INFO *)&record[2688];
			error_info_misc->error_slow = calc_status.error_slow;
			error_info_misc->error_ts = calc_status.error_ts;
			error_info_misc->error_ua = calc_status.error_ua;
			error_info_misc->error_ub = calc_status.error_ub;
			error_info_misc->error_uc = calc_status.error_uc;
			error_info_misc->error_u0 = calc_status.error_u0;
			error_save(SHORT_SDB_FILE_PATH, &short_error_idx, &short_error_cnt);
		}
		if((ui_loaded == 1)&&(gnd_error_wave_load == 1))
		{
			error_read(GND_SDB_FILE_PATH, gnd_error_idx, &gnd_error_cnt);
			error_info_misc = (ERROR_INFO *)&error_bin[2688];
			error_info_misc->witch = 0;
			gnd_error_wave_load = 0;
		}
		if((ui_loaded == 1)&&(over_error_wave_load == 1))
		{
			error_read(OVER_SDB_FILE_PATH, over_error_idx, &over_error_cnt);
			error_info_misc = (ERROR_INFO *)&error_bin[2688];
			error_info_misc->witch = 1;
			over_error_wave_load = 0;
		}
		if((ui_loaded == 1)&&(drop_error_wave_load == 1))
		{
			error_read(DROP_SDB_FILE_PATH, drop_error_idx, &drop_error_cnt);
			error_info_misc = (ERROR_INFO *)&error_bin[2688];
			error_info_misc->witch = 2;
			drop_error_wave_load = 0;
		}
		if((ui_loaded == 1)&&(pt_error_wave_load == 1))
		{
			error_read(PT_SDB_FILE_PATH, pt_error_idx, &pt_error_cnt);
			error_info_misc = (ERROR_INFO *)&error_bin[2688];
			error_info_misc->witch = 3;
			pt_error_wave_load = 0;
		}
		if((ui_loaded == 1)&&(short_error_wave_load == 1))
		{
			error_read(SHORT_SDB_FILE_PATH, short_error_idx, &short_error_cnt);
			error_info_misc = (ERROR_INFO *)&error_bin[2688];
			error_info_misc->witch = 4;
			short_error_wave_load = 0;
		}
		if((ui_loaded == 1)&&(client_node_load == 1))
		{
			client_node_load = 0;
			client_nodes_read();
			mqtt_start_req = 1;
			settings_loaded = 1;
			disp_node_page_idx = 1;
			disp_node_page_cnt = (client_nodes_cnt+7)/8;
			disp_node_page_update = 1;
		}
		if((ui_loaded == 1)&&(save_settings_req == 1)&&(settings_loaded == 1))
		{
			save_settings_req = 0;
			rt_kprintf("settings save req\n");
			set_settings();
			if(save_request_reset == 1)
			{
				display_data_disable = 0;
				save_request_reset = 0;
				rt_hw_cpu_reset();
			}
		}
		if((ui_loaded == 1)&&(save_nodelist_req == 1)&&(settings_loaded == 1))
		{
			save_nodelist_req = 0;
			client_nodes_write();
		}
	}
}

static int misc_thread_init(void)
{
    rt_err_t err;
	
		misc_thread_stack = rt_memheap_alloc(&system_heap, MISC_THREAD_STACK_SIZE);

    err = rt_thread_init(&misc_thread, "MISC", misc_thread_entry, RT_NULL,
           misc_thread_stack, MISC_THREAD_STACK_SIZE, MISC_THREAD_PRIO, 10);
    if(err != RT_EOK)
    {
        rt_kprintf("Failed to create MISC thread");
        return -1;
    }
    rt_thread_startup(&misc_thread);
		
		lazy_thread_stack = rt_memheap_alloc(&system_heap, LAZY_THREAD_STACK_SIZE);

    err = rt_thread_init(&lazy_thread, "LAZY", lazy_thread_entry, RT_NULL,
           lazy_thread_stack, LAZY_THREAD_STACK_SIZE, LAZY_THREAD_PRIO, 10);
    if(err != RT_EOK)
    {
        rt_kprintf("Failed to create LAZY thread");
        return -1;
    }
    rt_thread_startup(&lazy_thread);

    return 0;
}
INIT_ENV_EXPORT(misc_thread_init);
