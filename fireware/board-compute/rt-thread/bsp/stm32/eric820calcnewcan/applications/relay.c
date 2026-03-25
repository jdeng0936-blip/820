/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <rtconfig.h>
#include <rtthread.h>
#include <rthw.h>
#include "arm_math.h"
//#include "table_fft.h"
#include <math.h>
#include <string.h>
#include <dfs.h>
#include <dfs_fs.h>
//#include <dfs_posix.h>
#include <mbedtls/md5.h>
#define MISC_THREAD_STACK_SIZE 8192
#define MISC_THREAD_PRIO 28
static rt_uint8_t misc_thread_stack[MISC_THREAD_STACK_SIZE];
static struct rt_thread misc_thread;
extern RTC_HandleTypeDef RTC_Handler;
extern volatile uint8_t gnd_relay_work;
volatile uint8_t relay_open = 0;
extern uint8_t error_line;
extern uint8_t error_line_backup;
extern union LINE_STATUS line_status;
extern volatile uint8_t gnd_err_clr;
extern volatile uint8_t bc_selstart_req;
extern CALC_STATUS calc_status;
extern TIM_HandleTypeDef htim12;
#define V_ZERO			0x00
#define V_DROP			0x01
#define V_NORMAL		0x02
#define V_OVER			0x03

#define LINE_A			0x01
#define LINE_B			0x02
#define LINE_C			0x03

#define UPDATE_CMD		0x982055AA

extern volatile uint8_t file_write_busy;
extern volatile uint8_t cfgfile_write_busy;
extern uint16_t packet_cnt;
extern uint16_t packet_idx;
FILE * firmwaref;
volatile uint8_t update_start = 0;
extern uint8_t file_buf[];
volatile uint8_t relay_work_finshed = 1;
volatile uint8_t gnd_err_release = 0;

extern float sud_ua;
extern float sud_ub;
extern float sud_uc;
extern float sud_u0;

extern float stable_ua;
extern float stable_ub;
extern float stable_uc;
extern float stable_u0;
extern uint16_t packet_len;

extern unsigned char md5sum2[];
extern volatile uint8_t md5_result;
extern volatile uint8_t md5_result_ready;

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
char f1[33] = {0};
static void misc_thread_entry(void *parameter)
{
	uint8_t relay_perm = 0;
	char str[64];
	while(1)
	{
		rt_thread_mdelay(1);
		if((relay_open == 1)&&(relay_work_finshed == 1))
		{
			relay_work_finshed = 0;
			relay_open = 0;
			rt_kprintf("relay work start\n");
			gnd_relay_work = 1;
			rt_thread_mdelay(25);
			bc_selstart_req = 1;
			if(fast_error_line == LINE_A)
			{
				error_line = LINE_A;
				error_line_backup = 0xA0;
				//open relay a
				HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_SET);
				rt_kprintf("a gnd\n");
			}
			else if(fast_error_line == LINE_B)
			{
				error_line = LINE_B;
				error_line_backup = 0xB0;
				//open relay b
				HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_SET);
				rt_kprintf("b gnd\n");
			}
			else if(fast_error_line == LINE_C)
			{
				error_line = LINE_C;
				error_line_backup = 0xC0;
				//open relay c
				HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_SET);
				rt_kprintf("c gnd\n");
			}
			rt_thread_mdelay(40);
			if(((fast_error_line == LINE_A) && ((sud_ub < (float)calc_status.settings.v_over) || (sud_uc < (float)calc_status.settings.v_over)))
				&&((fast_error_line == LINE_B) && ((sud_ua < (float)calc_status.settings.v_over) || (sud_uc < (float)calc_status.settings.v_over)))
					&&((fast_error_line == LINE_C) && ((sud_ua < (float)calc_status.settings.v_over) || (sud_ub < (float)calc_status.settings.v_over))))
			{
				rt_kprintf("wrong relay all off\n");
				HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
				gnd_relay_work = 0;
				rt_thread_mdelay(1000);
				//relay_work_finshed = 1;
				gnd_err_release = 1;
			}
			else
			{
				//open relay 4
				HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
				rt_kprintf("relay 4 on\n");
				rt_thread_mdelay(6000);
				//off relay 4
				HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
				rt_kprintf("relay all off for test\n");
				rt_thread_mdelay(2000);
				
				sprintf(str,"%.2f, %.2f, %.2f, %.2f\n", stable_ua, stable_ub, stable_uc, stable_u0);
				rt_kprintf(str);
				
				if((error_line == LINE_A)&&(stable_ua<(float)calc_status.settings.v_drop)&&(stable_ub>(float)calc_status.settings.v_over)&&(stable_uc>(float)calc_status.settings.v_over)&&(stable_u0>(float)calc_status.settings.u0_limt))
				{
					HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_SET);
					HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
				}
				else if((error_line == LINE_B)&&(stable_ub<(float)calc_status.settings.v_drop)&&(stable_ua>(float)calc_status.settings.v_over)&&(stable_uc>(float)calc_status.settings.v_over)&&(stable_u0>(float)calc_status.settings.u0_limt))
				{
					HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_SET);
					HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
				}
				else if((error_line == LINE_C)&&(stable_uc<(float)calc_status.settings.v_drop)&&(stable_ua>(float)calc_status.settings.v_over)&&(stable_ub>(float)calc_status.settings.v_over)&&(stable_u0>(float)calc_status.settings.u0_limt))
				{
					HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_SET);
					HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
				}
				else
				{
					HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
					rt_kprintf("relay off\r\n");
					gnd_relay_work = 0;
					//relay_work_finshed = 1;
				}
				rt_thread_mdelay(1000);
				gnd_err_release = 1;
			}
		}
		
		if(HAL_GPIO_ReadPin(INPUT1_GPIO_Port, INPUT1_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x01;
		}
		else
		{
			calc_status.in_status &=~ 0x01;
		}
		
		if(HAL_GPIO_ReadPin(INPUT2_GPIO_Port, INPUT2_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x02;
		}
		else
		{
			calc_status.in_status &=~ 0x02;
		}
		
		if(HAL_GPIO_ReadPin(INPUT3_GPIO_Port, INPUT3_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x04;
		}
		else
		{
			calc_status.in_status &=~ 0x04;
		}
		
		if(HAL_GPIO_ReadPin(INPUT4_GPIO_Port, INPUT4_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x08;
		}
		else
		{
			calc_status.in_status &=~ 0x08;
		}
		
		if(HAL_GPIO_ReadPin(INPUT5_GPIO_Port, INPUT5_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x10;
		}
		else
		{
			calc_status.in_status &=~ 0x10;
		}
		
		if(HAL_GPIO_ReadPin(INPUT6_GPIO_Port, INPUT6_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x20;
		}
		else
		{
			calc_status.in_status &=~ 0x20;
		}
		
		if(HAL_GPIO_ReadPin(INPUT7_GPIO_Port, INPUT7_Pin) == GPIO_PIN_RESET)
		{
			calc_status.in_status |= 0x40;
		}
		else
		{
			calc_status.in_status &=~ 0x40;
		}
		
		if(HAL_GPIO_ReadPin(OUT1_GPIO_Port, OUT1_Pin) == GPIO_PIN_SET)
		{
			calc_status.out_status |= 0x01;
		}
		else
		{
			calc_status.out_status &=~ 0x01;
		}
		
		if(HAL_GPIO_ReadPin(OUT2_GPIO_Port, OUT2_Pin) == GPIO_PIN_SET)
		{
			calc_status.out_status |= 0x02;
		}
		else
		{
			calc_status.out_status &=~ 0x02;
		}
		
		if(HAL_GPIO_ReadPin(OUT3_GPIO_Port, OUT3_Pin) == GPIO_PIN_SET)
		{
			calc_status.out_status |= 0x04;
		}
		else
		{
			calc_status.out_status &=~ 0x04;
		}
		
		if(HAL_GPIO_ReadPin(OUT4_GPIO_Port, OUT4_Pin) == GPIO_PIN_SET)
		{
			calc_status.out_status |= 0x08;
		}
		else
		{
			calc_status.out_status &=~ 0x08;
		}
		
		if(HAL_GPIO_ReadPin(OUT5_GPIO_Port, OUT5_Pin) == GPIO_PIN_SET)
		{
			calc_status.out_status |= 0x10;
		}
		else
		{
			calc_status.out_status &=~ 0x10;
		}
		if(file_write_busy == 1)
		{
			if(packet_idx == 0)
			{
				rt_kprintf("create file\n");
				firmwaref = fopen("rtthread.bin", "wb+");
				if(firmwaref == NULL)
				{
					rt_kprintf("create file rtthread.bin failed!\n");
				}
				else
				{
					fseek(firmwaref, 0, SEEK_SET);
				}
			}
			fwrite(file_buf, 1, packet_len, firmwaref);
			fflush(firmwaref);
			rt_kprintf("write file %d %d\n", packet_idx, packet_cnt);
			packet_idx++;
			if(packet_idx == packet_cnt)
			{
				fclose(firmwaref);
				unsigned char md5sum1[16];
				md5_file("/rtthread.bin", md5sum1);
				if(memcmp(md5sum1,md5sum2,16) != 0)
				{
					md5_result = 0x00;
				}
				else
				{
					md5_result = 0x80;
				}
				md5_result_ready = 1;
				rt_thread_mdelay(3000);
				update_start = 1;
				rt_kprintf("close file\n");
			}
			file_write_busy = 0;
		}
		if(cfgfile_write_busy == 1)
		{
			if(packet_idx == 0)
			{
				rt_kprintf("create file\n");
				firmwaref = fopen("cancfg.msg", "wb+");
				if(firmwaref == NULL)
				{
					rt_kprintf("create file cancfg.msg failed!\n");
				}
				else
				{
					fseek(firmwaref, 0, SEEK_SET);
				}
			}
			fwrite(file_buf, 1, packet_len, firmwaref);
			fflush(firmwaref);
			rt_kprintf("write file %d %d\n", packet_idx, packet_cnt);
			packet_idx++;
			if(packet_idx == packet_cnt)
			{
				fclose(firmwaref);
				rt_kprintf("close file\n");
				rt_thread_mdelay(1000);
				rt_hw_cpu_reset();
			}
			cfgfile_write_busy = 0;
		}
		if(update_start == 1)
		{
			update_start = 0;
			HAL_RTCEx_BKUPWrite(&RTC_Handler, RTC_BKP_DR2, UPDATE_CMD);
			rt_thread_mdelay(1000);
			rt_hw_cpu_reset();
		}
		if(gnd_err_clr == 1)
		{
			gnd_err_clr = 0;
			rt_thread_mdelay(5000);
			fast_error = 0;
			gnd_relay_work = 0;
			rt_kprintf("err clr\n");
		}
		if(gnd_err_release == 1)
		{
			gnd_err_release = 0;
			rt_thread_mdelay(5000);
			relay_work_finshed = 1;
			rt_kprintf("relay err release\n");
		}
		if(settings_changed == 1)
		{
			rt_kprintf("save settings\n");
			settings_changed = 0;
			save_settings();
		}
	}
}

static int misc_thread_init(void)
{
	rt_err_t err;

	err = rt_thread_init(&misc_thread, "MISC", misc_thread_entry, RT_NULL,
			 &misc_thread_stack[0], sizeof(misc_thread_stack), MISC_THREAD_PRIO, 10);
	if(err != RT_EOK)
	{
		rt_kprintf("Failed to create MISC thread");
		return -1;
	}
	rt_thread_startup(&misc_thread);

	return 0;
}
INIT_ENV_EXPORT(misc_thread_init);
