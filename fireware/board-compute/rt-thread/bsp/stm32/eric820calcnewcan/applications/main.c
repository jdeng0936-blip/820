/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-18     zylx         first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arm_math.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
#include "main.h"
#include "at24cxx.h"

#define AD7606_BASE		0x6C000000
#define UPDATE_CMD		0x982055AA

#define V_ZERO			0x00
#define V_DROP			0x01
#define V_NORMAL		0x02
#define V_OVER			0x03


#define LINE_A			0x01
#define LINE_B			0x02
#define LINE_C			0x03

volatile uint8_t delay_cnt = 0;

void delay_50ns(void)
{
	for(delay_cnt =0;delay_cnt<10;delay_cnt++);
}

at24cxx_device_t i2c_dev = RT_NULL;

extern RTC_HandleTypeDef RTC_Handler;
extern CALC_STATUS calc_status;
union LINE_STATUS line_status;
CAN_HandleTypeDef hcan1;
SPI_HandleTypeDef hspi4;
DMA_HandleTypeDef hdma_spi4_rx;
DMA_HandleTypeDef hdma_spi4_tx;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim12;
DMA_HandleTypeDef hdma_memtomem_dma2_stream0;
SRAM_HandleTypeDef hsram1;

rt_sem_t spi_start_sem;
rt_mq_t adc_data_mq;
rt_mq_t wave_data_mq;
int16_t g_ad7606_buf[8];
uint8_t adc_idx = 0;
int8_t rx_wave_data[8];
extern int8_t error_buf[];
uint8_t recheck_timer = 0;
//volatile uint8_t recheck = 0;

//uint8_t st_a_v_over = 0;
//uint8_t st_b_v_over = 0;
//uint8_t st_c_v_over = 0;

//uint8_t st_a_v_drop = 0;
//uint8_t st_b_v_drop = 0;
//uint8_t st_c_v_drop = 0;

int16_t g_rms_ua = 0;
int16_t g_rms_ub = 0;
int16_t g_rms_uc = 0;
int16_t g_rms_u0 = 0;

int16_t g_rms_ia = 0;
int16_t g_rms_ib = 0;
int16_t g_rms_ic = 0;
int16_t g_rms_i0 = 0;

int16_t rms_avg_ua = 0;
int16_t rms_avg_ub = 0;
int16_t rms_avg_uc = 0;
int16_t rms_avg_u0 = 0;

int16_t rms_avg_ia = 0;
int16_t rms_avg_ib = 0;
int16_t rms_avg_ic = 0;
int16_t rms_avg_i0 = 0;

float stable_ua;
float stable_ub;
float stable_uc;
float stable_u0;

float sud_ua;
float sud_ub;
float sud_uc;
float sud_u0;

float rms_result_ua = 0.0f;
float rms_result_ub = 0.0f;
float rms_result_uc = 0.0f;
float rms_result_u0 = 0.0f;
float rms_result_ia = 0.0f;
float rms_result_ib = 0.0f;
float rms_result_ic = 0.0f;
float rms_result_i0 = 0.0f;

//float rms_ratio_ua = 110.470001f;
//float rms_ratio_ub = 109.440002f;
//float rms_ratio_uc = 109.650002f;
//float rms_ratio_u0 = 110.059998f;
//float rms_ratio_ia = 2000.0f;
//float rms_ratio_ib = 2000.0f;
//float rms_ratio_ic = 2000.0f;
//float rms_ratio_i0 = 4000.0f;

int16_t adc_data_ua[256] = {0};
int16_t adc_data_ub[256] = {0};
int16_t adc_data_uc[256] = {0};
int16_t adc_data_u0[256] = {0};

int16_t adc_data_ia[256] = {0};
int16_t adc_data_ib[256] = {0};
int16_t adc_data_ic[256] = {0};
int16_t adc_data_i0[256] = {0};

int8_t save_backup_buffer[8*384] = {0};
int8_t backup_buffer[8*384] = {0};
uint16_t backup_data_w_idx = 0;
uint16_t backup_data_w_cnt = 0;
volatile uint8_t backup_data_start = 0;
volatile uint8_t backup_data_end = 0;
volatile uint8_t error_line = 0;
volatile uint8_t error_line_backup = 0;
uint16_t adc_data_w_idx = 0;
uint8_t adc_data_calc_idx = 0;
uint8_t backup_idx = 0;
float v_normal;
uint8_t last_ls = 0xFF;
uint8_t error_ls;
volatile uint8_t line_normal_acq = 0;
//volatile uint8_t line_normal_cal = 0;
//uint32_t line_normal_timer = 0;
volatile uint8_t gnd_relay_work = 0;
extern volatile uint8_t bc_selstart_req;
extern volatile uint8_t spi_dma_rx_end;
extern volatile uint8_t spi_dma_tx_end;
extern volatile uint8_t error_data_ready;

volatile uint8_t over_err_cnt = 0;
volatile uint8_t drop_err_cnt = 0;
volatile uint8_t pt_err_cnt = 0;
volatile uint8_t short_err_cnt = 0;
			
int16_t g_rms_array_ua[12];
int16_t g_rms_array_ub[12];
int16_t g_rms_array_uc[12];
int16_t g_rms_array_u0[12];
int16_t g_rms_array_ia[12];
int16_t g_rms_array_ib[12];
int16_t g_rms_array_ic[12];
int16_t g_rms_array_i0[12];

uint8_t rms_array_w_idx = 0;

extern uint8_t sendbuf1[];
extern uint8_t sendbuf2[];
extern volatile uint8_t relay_open;

static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_FMC_Init(void);
static void MX_SPI4_Init(void);
static void MX_CAN1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM12_Init(void);

void ad7606_dma_cplt(DMA_HandleTypeDef *hdma);
static void wave_thread_entry(void *parameter);
void check_proc(uint8_t line);
extern void get_settings(void);
void save_settings(void);
volatile uint8_t settings_changed = 0;

#define VOLTAGE_OVER		0xE1
#define VOLTAGE_DROP		0xE2
#define PT_ERROR				0xE3
#define SHORT_ERROR			0xE4

volatile uint8_t fast_error = 0;
uint8_t fast_error_line = 0;
time_t fast_error_ts = 0;

volatile uint8_t slow_error = 0;
volatile uint8_t slow_error_ack = 0;
uint8_t slow_gnd_line = 0;
uint8_t slow_error_type = 0;
time_t slow_error_ts = 0;
volatile uint8_t i2c_check_ok = 0;

void fast_judge(float ua, float ub, float uc, float u0)
{
	if((calc_status.in_status & 0x01) == 0)
		return;
	if((u0 > (float)calc_status.settings.u0_limt)&&(fast_error == 0))
	{
		if((ua < (float)calc_status.settings.v_drop) && (ub > (float)calc_status.settings.v_over) && (uc > (float)calc_status.settings.v_over))
		{
			//a gnd
			//rt_kprintf("fast a gnd\n");
			fast_error_line = LINE_A;
			fast_error = 1;
		}
		if((ub < (float)calc_status.settings.v_drop) && (ua > (float)calc_status.settings.v_over) && (uc > (float)calc_status.settings.v_over))
		{
			//b gnd
			//rt_kprintf("fast b gnd\n");
			fast_error_line = LINE_B;
			fast_error = 1;
		}
		if((uc < (float)calc_status.settings.v_drop) && (ua > (float)calc_status.settings.v_over) && (ub > (float)calc_status.settings.v_over))
		{
			//c gnd
			//rt_kprintf("fast c gnd\n");
			fast_error_line = LINE_C;
			fast_error = 1;
		}
		if(fast_error == 1)
		{
			backup_data_start = 1;
			backup_data_w_cnt = 0;
			get_timestamp(&calc_status.error_ts);
			//triger event gnd error
			//bc_selstart_req = 1;
			HAL_TIM_PWM_Start_IT(&htim12, TIM_CHANNEL_2);
			relay_open = 1;
			calc_status.error_ua = ua;
			calc_status.error_ub = ub;
			calc_status.error_uc = uc;
			calc_status.error_u0 = u0;
			//rt_kprintf("triger event gnd error\n");
		}
	}
}

void slow_judge(float ua, float ub, float uc, float u0)
{
	uint8_t drop_cnt = 0;
	uint8_t over_cnt = 0;
	if((calc_status.in_status & 0x01) == 0)
		return;
	if(u0 > (float)calc_status.settings.u0_limt)
	{
		//gnd
		if((ua < (float)calc_status.settings.v_drop) && (ub > (float)calc_status.settings.v_over) && (uc > (float)calc_status.settings.v_over))
		{
			//a gnd
			slow_gnd_line = LINE_A;
			//rt_kprintf("slow a gnd\n");
		}
		if((ub < (float)calc_status.settings.v_drop) && (ua > (float)calc_status.settings.v_over) && (uc > (float)calc_status.settings.v_over))
		{
			//b gnd
			slow_gnd_line = LINE_B;
			//rt_kprintf("slow b gnd\n");
		}
		if((uc < (float)calc_status.settings.v_drop) && (ua > (float)calc_status.settings.v_over) && (ub > (float)calc_status.settings.v_over))
		{
			//c gnd
			slow_gnd_line = LINE_C;
			//rt_kprintf("slow c gnd\n");
		}
		if((fast_error == 0)&&(slow_error == 0))
		{
			if(ua < (float)calc_status.settings.v_drop)
			{
				drop_cnt++;
			}
			if(ub < (float)calc_status.settings.v_drop)
			{
				drop_cnt++;
			}
			if(uc < (float)calc_status.settings.v_drop)
			{
				drop_cnt++;
			}
			
			if(ua > (float)calc_status.settings.v_over)
			{
				over_cnt++;
			}
			if(ub > (float)calc_status.settings.v_over)
			{
				over_cnt++;
			}
			if(uc > (float)calc_status.settings.v_over)
			{
				over_cnt++;
			}
			if((line_normal_acq == 1)&&(((over_cnt == 1)&&(drop_cnt == 2))||((over_cnt == 0)&&(drop_cnt == 3))))
			{
				rt_kprintf("short error\n");
				short_err_cnt++;
				over_err_cnt = 0;
				drop_err_cnt = 0;
				pt_err_cnt = 0;
				if(short_err_cnt == 5)
				{
					slow_error_type = SHORT_ERROR;
					backup_data_start = 1;
					backup_data_w_cnt = 0;
					get_timestamp(&calc_status.error_ts);
					calc_status.error_ua = ua;
					calc_status.error_ub = ub;
					calc_status.error_uc = uc;
					calc_status.error_u0 = u0;
					slow_error = 1;
				}
			}
		}
	}
	if((fast_error == 0)&&(slow_error == 0)&&!((drop_cnt == 1)&&(over_cnt == 1)))
	{
		slow_gnd_line = 0;
		//check slow error
		if(((ua > (float)calc_status.settings.v_over) || (ub > (float)calc_status.settings.v_over) || (uc > (float)calc_status.settings.v_over))
			&&!((ua < (float)calc_status.settings.v_drop) || (ub < (float)calc_status.settings.v_drop) || (uc < (float)calc_status.settings.v_drop)))
		{
			//voltage over
			over_err_cnt++;
			short_err_cnt = 0;
			drop_err_cnt = 0;
			pt_err_cnt = 0;
			if(over_err_cnt == 24)
			{
				rt_kprintf("voltage over\n");
				slow_error_type = VOLTAGE_OVER;
				slow_error = 1;
			}
		}
		if(
				(((ua < (float)calc_status.settings.v_drop)&&(ua > (float)calc_status.settings.v_zero)) 
						|| ((ub < (float)calc_status.settings.v_drop)&&(ub > (float)calc_status.settings.v_zero)) 
							|| ((uc < (float)calc_status.settings.v_drop)&&(uc > (float)calc_status.settings.v_zero)))
			&&(((ua > (float)calc_status.settings.v_drop)&&(ua < (float)calc_status.settings.v_over)) 
				|| ((ub > (float)calc_status.settings.v_drop)&&(ub < (float)calc_status.settings.v_over)) 
					|| ((uc > (float)calc_status.settings.v_drop)&&(uc < (float)calc_status.settings.v_over))))
		{
			if(line_normal_acq == 1)
			{
				//voltage drop
				drop_err_cnt++;
				short_err_cnt = 0;
				over_err_cnt = 0;
				pt_err_cnt = 0;
				if(drop_err_cnt == 24)
				{
					slow_error_type = VOLTAGE_DROP;
					rt_kprintf("voltage drop\n");
					slow_error = 1;
				}
			}
		}
		if(((ua < (float)calc_status.settings.v_zero) || (ub < (float)calc_status.settings.v_zero) || (uc < (float)calc_status.settings.v_zero))
			&&((ua > (float)calc_status.settings.v_drop) || (ub > (float)calc_status.settings.v_drop) || (uc > (float)calc_status.settings.v_drop)))
		{
			//pt error
			pt_err_cnt++;
			over_err_cnt = 0;
			drop_err_cnt = 0;
			short_err_cnt = 0;
			if(pt_err_cnt == 5)
			{
				slow_error_type = PT_ERROR;
				rt_kprintf("pt error\n");
				slow_error = 1;
			}
		}
		if(slow_error == 1)
		{
			backup_data_start = 1;
			backup_data_w_cnt = 0;
			get_timestamp(&calc_status.error_ts);
			calc_status.error_ua = ua;
			calc_status.error_ub = ub;
			calc_status.error_uc = uc;
			calc_status.error_u0 = u0;
			//triger event voltage error
			rt_kprintf("triger event voltage error\n");
		}
		if((ua > (float)calc_status.settings.v_drop)&&(ua < (float)calc_status.settings.v_over)
			&&(ub > (float)calc_status.settings.v_drop)&&(ub < (float)calc_status.settings.v_over)
				&&(uc > (float)calc_status.settings.v_drop)&&(uc < (float)calc_status.settings.v_over))
		{
			line_normal_acq = 1;
		}
		if((ua < (float)calc_status.settings.v_zero)&&(ub < (float)calc_status.settings.v_zero)&&(uc < (float)calc_status.settings.v_zero))
		{
			line_normal_acq = 0;
		}
	}
	else
	{
		slow_gnd_line = 0;
		if(((ua < (float)calc_status.settings.v_zero) && (ub < (float)calc_status.settings.v_zero) && (uc < (float)calc_status.settings.v_zero))
			||((ua > (float)calc_status.settings.v_drop) && (ub > (float)calc_status.settings.v_drop) && (uc > (float)calc_status.settings.v_drop)
					&&(ua < (float)calc_status.settings.v_over) && (ub < (float)calc_status.settings.v_over) && (uc < (float)calc_status.settings.v_over)))
		{
			over_err_cnt = 0;
			drop_err_cnt = 0;
			pt_err_cnt = 0;
			short_err_cnt = 0;
			
			if(slow_error_ack == 1)
			{
				slow_error = 0;
				slow_error_type = 0;
				slow_error_ack = 0;
			}
		}
	}
}

int main(void)
{
	int16_t rx_adc_data[8];
	int8_t rx_save_data[8];
	int32_t rms_sum_ua,rms_sum_ub,rms_sum_uc,rms_sum_u0,rms_sum_ia,rms_sum_ib,rms_sum_ic,rms_sum_i0;
	
	spi_start_sem = rt_sem_create("spi4_st", 0, RT_IPC_FLAG_FIFO);
	adc_data_mq = rt_mq_create("adc_mq", 16, 64, RT_IPC_FLAG_FIFO);
	wave_data_mq = rt_mq_create("wav_mq", 6, 8, RT_IPC_FLAG_FIFO);
	
	i2c_dev = at24cxx_init("i2c1", 0);
	
	while(RT_EOK != at24cxx_check(i2c_dev))
		rt_thread_mdelay(100);
	
	i2c_check_ok = 1;
	
	if(dfs_mount("W25Q256", "/", "elm", 0, 0) != 0)
	{
		rt_kprintf("spi flash mount failed\n");
		if(dfs_mkfs("elm", "W25Q256") == 0)
		{
			rt_kprintf("spi flash formated\n");
			dfs_mount("W25Q256", "/", "elm", 0, 0);
		}
	}
	
	get_settings();
	
	MX_GPIO_Init();
  MX_DMA_Init();
  MX_FMC_Init();
	MX_SPI4_Init();
	MX_TIM3_Init();
	MX_CAN1_Init();
  MX_TIM12_Init();
	
	struct timeval tv = { 0 };
	gettimeofday(&tv, RT_NULL);
	
	//HAL_RTCEx_BKUPWrite(&RTC_Handler, RTC_BKP_DR2, UPDATE_CMD);
	
	HAL_GPIO_WritePin(AD_RESET_GPIO_Port, AD_RESET_Pin, GPIO_PIN_SET);
	rt_thread_mdelay(10);
	HAL_GPIO_WritePin(AD_RESET_GPIO_Port, AD_RESET_Pin, GPIO_PIN_RESET);
	
	HAL_DMA_RegisterCallback(&hdma_memtomem_dma2_stream0, HAL_DMA_XFER_CPLT_CB_ID, ad7606_dma_cplt);
	rt_thread_mdelay(3000);
	HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_1);
	
	while (1)
	{
		if(rt_mq_recv(adc_data_mq, rx_adc_data, 16, 1) > 0)
		{
			adc_data_ua[adc_data_w_idx] = rx_adc_data[4];
			adc_data_ub[adc_data_w_idx] = rx_adc_data[5];
			adc_data_uc[adc_data_w_idx] = rx_adc_data[7];
			adc_data_u0[adc_data_w_idx] = rx_adc_data[6];
			adc_data_ia[adc_data_w_idx] = rx_adc_data[0];
			adc_data_ib[adc_data_w_idx] = rx_adc_data[1];
			adc_data_ic[adc_data_w_idx] = rx_adc_data[2];
			adc_data_i0[adc_data_w_idx] = rx_adc_data[3];
			
			//fast check gnd
			adc_data_calc_idx++;
			if(adc_data_calc_idx == 64)
			{
				adc_data_calc_idx = 0;
				//check rms per 5ms
				arm_rms_q15(adc_data_ua, 256, &g_rms_ua);
				arm_rms_q15(adc_data_ub, 256, &g_rms_ub);
				arm_rms_q15(adc_data_uc, 256, &g_rms_uc);
				arm_rms_q15(adc_data_u0, 256, &g_rms_u0);
				arm_rms_q15(adc_data_ia, 256, &g_rms_ia);
				arm_rms_q15(adc_data_ib, 256, &g_rms_ib);
				arm_rms_q15(adc_data_ic, 256, &g_rms_ic);
				arm_rms_q15(adc_data_i0, 256, &g_rms_i0);
				
				sud_ua = (float)g_rms_ua / calc_status.settings.rms_ratio_ua;
				sud_ub = (float)g_rms_ub / calc_status.settings.rms_ratio_ub;
				sud_uc = (float)g_rms_uc / calc_status.settings.rms_ratio_uc;
				sud_u0 = (float)g_rms_u0 / calc_status.settings.rms_ratio_u0;
				
				fast_judge(sud_ua, sud_ub, sud_uc, sud_u0);
			}
			//slow check other
			adc_data_w_idx++;
			if(adc_data_w_idx == 256)
			{
				adc_data_w_idx = 0;
				//save rms per 20ms
				g_rms_array_ua[rms_array_w_idx] = g_rms_ua;
				g_rms_array_ub[rms_array_w_idx] = g_rms_ub;
				g_rms_array_uc[rms_array_w_idx] = g_rms_uc;
				g_rms_array_u0[rms_array_w_idx] = g_rms_u0;
				g_rms_array_ia[rms_array_w_idx] = g_rms_ia;
				g_rms_array_ib[rms_array_w_idx] = g_rms_ib;
				g_rms_array_ic[rms_array_w_idx] = g_rms_ic;
				g_rms_array_i0[rms_array_w_idx] = g_rms_i0;
				
				rms_array_w_idx++;
				if(rms_array_w_idx == 12)
					rms_array_w_idx = 0;
				//calc sum
				rms_sum_ua = 0;
				rms_sum_ub = 0;
				rms_sum_uc = 0;
				rms_sum_u0 = 0;
				rms_sum_ia = 0;
				rms_sum_ib = 0;
				rms_sum_ic = 0;
				rms_sum_i0 = 0;
				for(int i=0;i<12;i++)
				{
					rms_sum_ua += g_rms_array_ua[i];
					rms_sum_ub += g_rms_array_ub[i];
					rms_sum_uc += g_rms_array_uc[i];
					rms_sum_u0 += g_rms_array_u0[i];
					rms_sum_ia += g_rms_array_ia[i];
					rms_sum_ib += g_rms_array_ib[i];
					rms_sum_ic += g_rms_array_ic[i];
					rms_sum_i0 += g_rms_array_i0[i];
				}
				rms_avg_ua = rms_sum_ua/12;
				rms_avg_ub = rms_sum_ub/12;
				rms_avg_uc = rms_sum_uc/12;
				rms_avg_u0 = rms_sum_u0/12;
				rms_avg_ia = rms_sum_ia/12;
				rms_avg_ib = rms_sum_ib/12;
				rms_avg_ic = rms_sum_ic/12;
				rms_avg_i0 = rms_sum_i0/12;
				
				stable_ua = (float)rms_avg_ua / calc_status.settings.rms_ratio_ua;
				stable_ub = (float)rms_avg_ub / calc_status.settings.rms_ratio_ub;
				stable_uc = (float)rms_avg_uc / calc_status.settings.rms_ratio_uc;
				stable_u0 = (float)rms_avg_u0 / calc_status.settings.rms_ratio_u0;
				
				slow_judge(stable_ua, stable_ub, stable_uc, stable_u0);
				
				rms_result_ua = (float)(rms_avg_ua * calc_status.settings.pt) / calc_status.settings.rms_ratio_ua;
				rms_result_ub = (float)(rms_avg_ub * calc_status.settings.pt) / calc_status.settings.rms_ratio_ub;
				rms_result_uc = (float)(rms_avg_uc * calc_status.settings.pt) / calc_status.settings.rms_ratio_uc;
				rms_result_u0 = (float)rms_avg_u0 / calc_status.settings.rms_ratio_u0;
				
				rms_result_ua /= 1000.0f;
				rms_result_ub /= 1000.0f;
				rms_result_uc /= 1000.0f;
				
				rms_result_ia = (float)(rms_avg_ia * calc_status.settings.ct) / calc_status.settings.rms_ratio_ia;
				rms_result_ib = (float)(rms_avg_ib * calc_status.settings.ct) / calc_status.settings.rms_ratio_ib;
				rms_result_ic = (float)(rms_avg_ic * calc_status.settings.ct) / calc_status.settings.rms_ratio_ic;
				rms_result_i0 = (float)(rms_avg_i0 * calc_status.settings.i0_ct) / calc_status.settings.rms_ratio_i0;
				
				memcpy((uint8_t*)&sendbuf1[384*6], &rms_result_ua, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4], &rms_result_ub, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*2], &rms_result_uc, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*3], &rms_result_u0, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*4], &rms_result_ia, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*5], &rms_result_ib, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*6], &rms_result_ic, 4);
				memcpy((uint8_t*)&sendbuf1[384*6+4*7], &rms_result_i0, 4);
				
				memcpy((uint8_t*)&sendbuf2[384*6], &rms_result_ua, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4], &rms_result_ub, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*2], &rms_result_uc, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*3], &rms_result_u0, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*4], &rms_result_ia, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*5], &rms_result_ib, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*6], &rms_result_ic, 4);
				memcpy((uint8_t*)&sendbuf2[384*6+4*7], &rms_result_i0, 4);
			}
			//error back wave
			backup_idx++;
			if(backup_idx == 8)
			{
				backup_idx = 0;
				if(backup_data_end == 0)
				{
					save_backup_buffer[backup_data_w_idx*8] = rx_adc_data[4]/256;
					save_backup_buffer[backup_data_w_idx*8+1] = rx_adc_data[5]/256;
					save_backup_buffer[backup_data_w_idx*8+2] = rx_adc_data[7]/256;
					save_backup_buffer[backup_data_w_idx*8+3] = rx_adc_data[6]/256;
					save_backup_buffer[backup_data_w_idx*8+4] = rx_adc_data[0]/256;
					save_backup_buffer[backup_data_w_idx*8+5] = rx_adc_data[1]/256;
					save_backup_buffer[backup_data_w_idx*8+6] = rx_adc_data[2]/256;
					save_backup_buffer[backup_data_w_idx*8+7] = rx_adc_data[3]/256;
					
					backup_data_w_idx++;
					if(backup_data_w_idx == 384)
						backup_data_w_idx = 0;
					if(backup_data_start == 1)
					{
						backup_data_w_cnt++;
						if(backup_data_w_cnt == 256)
						{
							for(int i=0;i<384;i++)
							{
								backup_buffer[i*8] = save_backup_buffer[backup_data_w_idx*8];
								backup_buffer[i*8+1] = save_backup_buffer[backup_data_w_idx*8+1];
								backup_buffer[i*8+2] = save_backup_buffer[backup_data_w_idx*8+2];
								backup_buffer[i*8+3] = save_backup_buffer[backup_data_w_idx*8+3];
								backup_buffer[i*8+4] = save_backup_buffer[backup_data_w_idx*8+4];
								backup_buffer[i*8+5] = save_backup_buffer[backup_data_w_idx*8+5];
								backup_buffer[i*8+6] = save_backup_buffer[backup_data_w_idx*8+6];
								backup_buffer[i*8+7] = save_backup_buffer[backup_data_w_idx*8+7];
								backup_data_w_idx++;
								if(backup_data_w_idx == 384)
									backup_data_w_idx = 0;
							}
							backup_data_end = 1;
							if(slow_error_type != 0)
							{
								for(int i=0;i<384;i++)
								{
									error_buf[i*7] = backup_buffer[8*i];
									error_buf[i*7+1] = backup_buffer[8*i+1];
									error_buf[i*7+2] = backup_buffer[8*i+2];
									error_buf[i*7+3] = backup_buffer[8*i+3];
									error_buf[i*7+4] = 0;
									error_buf[i*7+5] = 0;
									error_buf[i*7+6] = 0;
								}
								error_data_ready = 1;
							}
						}
					}
				}
			}
		}
	}
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 6249;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 3124;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOI_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, OUT2_Pin|OUT1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TEST_LED_GPIO_Port, TEST_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, AD_RESET_Pin|OUT5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(AD_RANGE_GPIO_Port, AD_RANGE_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, I2C1_SCL_Pin|I2C1_SDA_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : SPI4_NSS_Pin */
  GPIO_InitStruct.Pin = SPI4_NSS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SPI4_NSS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OUT3_Pin */
  GPIO_InitStruct.Pin = OUT3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OUT3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OUT2_Pin OUT1_Pin */
  GPIO_InitStruct.Pin = OUT2_Pin|OUT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

  /*Configure GPIO pin : PF6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PF7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : INPUT1_Pin INPUT2_Pin INPUT4_Pin */
  GPIO_InitStruct.Pin = INPUT1_Pin|INPUT2_Pin|INPUT4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : TEST_LED_Pin */
  GPIO_InitStruct.Pin = TEST_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TEST_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : AD_INT_Pin */
  GPIO_InitStruct.Pin = AD_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(AD_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : AD_RESET_Pin AD_RANGE_Pin */
  GPIO_InitStruct.Pin = AD_RESET_Pin|AD_RANGE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : OUT4_Pin */
  GPIO_InitStruct.Pin = OUT4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OUT4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : OUT5_Pin */
  GPIO_InitStruct.Pin = OUT5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OUT5_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : INPUT7_Pin */
  GPIO_InitStruct.Pin = INPUT7_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(INPUT7_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : INPUT6_Pin INPUT5_Pin INPUT3_Pin */
  GPIO_InitStruct.Pin = INPUT6_Pin|INPUT5_Pin|INPUT3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : I2C1_SCL_Pin */
  GPIO_InitStruct.Pin = I2C1_SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(I2C1_SCL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : I2C1_SDA_Pin */
  GPIO_InitStruct.Pin = I2C1_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(I2C1_SDA_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
	HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);
	
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/**
  * Enable DMA controller clock
  * Configure DMA for memory to memory transfers
  *   hdma_memtomem_dma2_stream0
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* Configure DMA request hdma_memtomem_dma2_stream0 on DMA2_Stream0 */
  hdma_memtomem_dma2_stream0.Instance = DMA2_Stream0;
  hdma_memtomem_dma2_stream0.Init.Channel = DMA_CHANNEL_0;
  hdma_memtomem_dma2_stream0.Init.Direction = DMA_MEMORY_TO_MEMORY;
  hdma_memtomem_dma2_stream0.Init.PeriphInc = DMA_PINC_ENABLE;
  hdma_memtomem_dma2_stream0.Init.MemInc = DMA_MINC_ENABLE;
  hdma_memtomem_dma2_stream0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_memtomem_dma2_stream0.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_memtomem_dma2_stream0.Init.Mode = DMA_NORMAL;
  hdma_memtomem_dma2_stream0.Init.Priority = DMA_PRIORITY_VERY_HIGH;
  hdma_memtomem_dma2_stream0.Init.FIFOMode = DMA_FIFOMODE_ENABLE;
  hdma_memtomem_dma2_stream0.Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
  hdma_memtomem_dma2_stream0.Init.MemBurst = DMA_MBURST_INC8;
  hdma_memtomem_dma2_stream0.Init.PeriphBurst = DMA_PBURST_INC8;
  if (HAL_DMA_Init(&hdma_memtomem_dma2_stream0) != HAL_OK)
  {
    Error_Handler( );
  }

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}
/* FMC initialization function */
static void MX_FMC_Init(void)
{

  /* USER CODE BEGIN FMC_Init 0 */

  /* USER CODE END FMC_Init 0 */

  FMC_NORSRAM_TimingTypeDef Timing = {0};
  
  /* USER CODE BEGIN FMC_Init 1 */

  /* USER CODE END FMC_Init 1 */

  /** Perform the SRAM1 memory initialization sequence
  */
  hsram1.Instance = FMC_NORSRAM_DEVICE;
  hsram1.Extended = FMC_NORSRAM_EXTENDED_DEVICE;
  /* hsram1.Init */
  hsram1.Init.NSBank = FMC_NORSRAM_BANK4;
  hsram1.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;
  hsram1.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;
  hsram1.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_16;
  hsram1.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;
  hsram1.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW;
  hsram1.Init.WrapMode = FMC_WRAP_MODE_DISABLE;
  hsram1.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;
  hsram1.Init.WriteOperation = FMC_WRITE_OPERATION_DISABLE;
  hsram1.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;
  hsram1.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;
  hsram1.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;
  hsram1.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;
  hsram1.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;
  hsram1.Init.PageSize = FMC_PAGE_SIZE_NONE;
  /* Timing */
  Timing.AddressSetupTime = 3;
  Timing.AddressHoldTime = 15;
  Timing.DataSetupTime = 6;
  Timing.BusTurnAroundDuration = 1;
  Timing.CLKDivision = 16;
  Timing.DataLatency = 17;
  Timing.AccessMode = FMC_ACCESS_MODE_A;
  /* ExtTiming */

  if (HAL_SRAM_Init(&hsram1, &Timing, NULL) != HAL_OK)
  {
    Error_Handler( );
  }
  /* USER CODE END FMC_Init 2 */
}
/**
  * @brief SPI4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI4_Init(void)
{

  /* USER CODE BEGIN SPI4_Init 0 */

  /* USER CODE END SPI4_Init 0 */

  /* USER CODE BEGIN SPI4_Init 1 */

  /* USER CODE END SPI4_Init 1 */
  /* SPI4 parameter configuration*/
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_SLAVE;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES;
  hspi4.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi4.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi4.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi4.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi4.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi4.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi4.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI4_Init 2 */

  /* USER CODE END SPI4_Init 2 */

}

/**
  * @brief This function handles TIM8 break interrupt and TIM12 global interrupt.
  */
void TIM8_BRK_TIM12_IRQHandler(void)
{
  /* USER CODE BEGIN TIM8_BRK_TIM12_IRQn 0 */

  /* USER CODE END TIM8_BRK_TIM12_IRQn 0 */
  HAL_TIM_IRQHandler(&htim12);
  /* USER CODE BEGIN TIM8_BRK_TIM12_IRQn 1 */

  /* USER CODE END TIM8_BRK_TIM12_IRQn 1 */
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if(htim == &htim12)
	{
		HAL_TIM_PWM_Stop_IT(&htim12, TIM_CHANNEL_2);
		TIM12->CNT = 0;
	}
}

void SPI_GPIO_EXTI_Callback(void)
{
	rt_sem_release(spi_start_sem);
}

void RST_GPIO_EXTI_Callback(void)
{
	//rt_hw_cpu_reset();
	HAL_SPI_DMAStop(&hspi4);
	__HAL_RCC_SPI4_FORCE_RESET();
	__HAL_RCC_SPI4_RELEASE_RESET();
	spi_dma_tx_end = 1;
	spi_dma_rx_end = 1;
	MX_SPI4_Init();
	rt_sem_control(spi_start_sem, RT_IPC_CMD_RESET, RT_NULL);
}

void ADC_GPIO_EXTI_Callback(void)
{
	HAL_DMA_Start_IT(&hdma_memtomem_dma2_stream0, (uint32_t)AD7606_BASE, (uint32_t)g_ad7606_buf, 8);
}

void DMA2_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream0_IRQn 0 */

  /* USER CODE END DMA2_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_memtomem_dma2_stream0);
  /* USER CODE BEGIN DMA2_Stream0_IRQn 1 */

  /* USER CODE END DMA2_Stream0_IRQn 1 */
}

void DMA2_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream1_IRQn 0 */

  /* USER CODE END DMA2_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi4_tx);
  /* USER CODE BEGIN DMA2_Stream1_IRQn 1 */

  /* USER CODE END DMA2_Stream1_IRQn 1 */
}

void DMA2_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream3_IRQn 0 */

  /* USER CODE END DMA2_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi4_rx);
  /* USER CODE BEGIN DMA2_Stream3_IRQn 1 */

  /* USER CODE END DMA2_Stream3_IRQn 1 */
}
void SPI4_IRQHandler(void)
{
  /* USER CODE BEGIN SPI4_IRQn 0 */

  /* USER CODE END SPI4_IRQn 0 */
  HAL_SPI_IRQHandler(&hspi4);
  /* USER CODE BEGIN SPI4_IRQn 1 */

  /* USER CODE END SPI4_IRQn 1 */
}
/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */
	CAN_FilterTypeDef  sFilterConfig;
  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 40;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_3TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = ENABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = ENABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
	
	sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdHigh = 0x0000;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdHigh = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
  {
    /* Filter configuration Error */
    Error_Handler();
  }
	
	if (HAL_CAN_Start(&hcan1) != HAL_OK)
  {
    /* Start Error */
		rt_hw_cpu_reset();
    Error_Handler();
  }
	
	if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    /* Notification Error */
    Error_Handler();
  }
	
  /* USER CODE END CAN1_Init 2 */

}
/**
  * @brief TIM12 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM12_Init(void)
{

  /* USER CODE BEGIN TIM12_Init 0 */

  /* USER CODE END TIM12_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM12_Init 1 */

  /* USER CODE END TIM12_Init 1 */
  htim12.Instance = TIM12;
  htim12.Init.Prescaler = 7999;
  htim12.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim12.Init.Period = 9999;
  htim12.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim12.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim12) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim12, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim12) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 140;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim12, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM12_Init 2 */

  /* USER CODE END TIM12_Init 2 */
  HAL_TIM_MspPostInit(&htim12);

}
void ad7606_dma_cplt(DMA_HandleTypeDef *hdma)
{
	rt_mq_send(adc_data_mq, g_ad7606_buf, 16);
	adc_idx++;
	if(adc_idx == 8)
	{
		adc_idx = 0;
		
		rx_wave_data[0] = g_ad7606_buf[4]/calc_status.settings.rms_ratio_ua/2;
		rx_wave_data[1] = g_ad7606_buf[5]/calc_status.settings.rms_ratio_ub/2;
		rx_wave_data[2] = g_ad7606_buf[7]/calc_status.settings.rms_ratio_uc/2;
		rx_wave_data[3] = g_ad7606_buf[0]/calc_status.settings.rms_ratio_ia/2;
		rx_wave_data[4] = g_ad7606_buf[1]/calc_status.settings.rms_ratio_ib/2;
		rx_wave_data[5] = g_ad7606_buf[2]/calc_status.settings.rms_ratio_ic/2;
		rt_mq_send(wave_data_mq, rx_wave_data, 6);
	}
	HAL_GPIO_WritePin(AD_RESET_GPIO_Port, AD_RESET_Pin, GPIO_PIN_SET);
	delay_50ns();
	HAL_GPIO_WritePin(AD_RESET_GPIO_Port, AD_RESET_Pin, GPIO_PIN_RESET);
}
extern volatile uint8_t relay_work_finshed;
void check_proc(uint8_t line)
{
	rt_kprintf("check_proc (%d,%d) (%d,%d) (%d,%d) %d\n", line_status.bits.la_st, (uint8_t)sud_ua, line_status.bits.lb_st, (uint8_t)sud_ub, line_status.bits.lc_st, (uint8_t)sud_uc, (uint8_t)sud_u0);
	if((gnd_relay_work == 0)&&(relay_work_finshed == 1))
	{
		gnd_relay_work = 1;
		HAL_TIM_PWM_Start_IT(&htim12, TIM_CHANNEL_2);
		bc_selstart_req = 1;
		rt_thread_mdelay(25);
		if(line == LINE_A)
		{
			error_line = LINE_A;
			error_line_backup = 0xA0;
			//open relay a
			HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_SET);
		}
		else if(line == LINE_B)
		{
			error_line = LINE_B;
			error_line_backup = 0xB0;
			//open relay b
			HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_SET);
		}
		else if(line == LINE_C)
		{
			error_line = LINE_C;
			error_line_backup = 0xC0;
			//open relay c
			HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_SET);
		}
//		relay_timer = 0;
		relay_open = 1;
	}
}
