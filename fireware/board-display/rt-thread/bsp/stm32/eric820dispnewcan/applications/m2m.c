#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
//#include <drivers/rt_drv_pwm.h>
#include <drv_spi.h>
#include "ui.h"
#include "main.h"
#include "user_mb_app.h"
#define M2M_THREAD_STACK_SIZE 8192
#define M2M_THREAD_PRIO 19
static rt_uint8_t m2m_thread_stack[M2M_THREAD_STACK_SIZE];
static struct rt_thread m2m_thread;
extern rt_mq_t wave_data_mq;
volatile uint8_t display_data_disable = 0;
#define DC		GET_PIN(I, 11)
#define RST		GET_PIN(I, 10)
#define SPICS		GET_PIN(E, 4)

SPI_HandleTypeDef hspi4;
DMA_HandleTypeDef hdma_spi4_rx;
DMA_HandleTypeDef hdma_spi4_tx;

extern volatile uint8_t ui_loaded;
volatile uint8_t error_wave_update = 0;
volatile uint8_t perm_error_clr_req = 0;

float rms_result_ua = 0.0f;
float rms_result_ub = 0.0f;
float rms_result_uc = 0.0f;
float rms_result_u0 = 0.0f;
float rms_result_ia = 0.0f;
float rms_result_ib = 0.0f;
float rms_result_ic = 0.0f;
float rms_result_i0 = 0.0f;

uint8_t file_buf[4096];
uint8_t cmdbuf[32];
uint8_t cmdackbuf[8];
int8_t wavebuf[384*6+4*8];
int8_t error_wavebuf[384*7];
volatile uint8_t disp_wave_update = 0;
volatile uint8_t calc_status_update = 0;
volatile uint8_t calc_status_ready = 0;
volatile uint8_t spi_dma_rx_end = 1;
volatile uint8_t spi_dma_tx_end = 1;
volatile uint8_t time_sync_req = 0;
volatile uint8_t cmd_calc_prepare_req = 0;
volatile uint8_t cmd_calc_v_done_req = 0;
volatile uint8_t cmd_calc_c_done_req = 0;
volatile uint8_t settings_changed = 0;
volatile uint8_t relay_op_req = 0;
extern SETTINGS settings;

VOL_ERROR_INFO over_error_info;
VOL_ERROR_INFO drop_error_info;
VOL_ERROR_INFO pt_error_info;
VOL_ERROR_INFO short_error_info;
extern volatile uint8_t sync_update_file;
extern volatile uint8_t sync_cancfg_file;

volatile uint16_t m2m_error_cnt = 0;
extern uint16_t client_nodes_cnt;
extern NODE client_nodes[];
extern volatile uint8_t disp_node_page_update;
extern volatile uint8_t update_start;
volatile uint8_t check_update_md5_req = 0;
volatile uint8_t md5_result_ready = 0;
volatile uint8_t md5_result = 0;
CALC_STATUS calc_status;
CALC_STATUS calc_status_buf;
REALTIME_INFO realtime_info;

volatile uint8_t set_file_md5_req = 0;

static uint16_t calc_crc(uint8_t *data, uint16_t length)
{
  uint16_t crc = 0u;
  while (length)
  {
		length--;
		crc = crc ^ ((uint16_t)*data++ << 8u);
		for (uint8_t i = 0u; i < 8u; i++)
		{
			if (crc & 0x8000u)
			{
				crc = (crc << 1u) ^ 0x1021u;
			}
			else
			{
				crc = crc << 1u;
			}
		}
  }
  return crc;
}

#define	CMD_REALTIME_WAVE_GET			0x01		//get realtime wave if ava
#define CMD_SETTINGS_GET					0x03		//get current settings
#define CMD_SETTINGS_SET					0x04		//set settings
#define CMD_ERROR_CNT							0x05		//check how many errors in storage
#define CMD_ERROR_DATA_GET				0x06		//get error
#define CMD_STATUS_GET						0x07		//get status
#define CMD_FW_INFO								0x08		//get firmware info
#define CMD_FILE_START						0x09		//prepare to send a file with name and size
#define CMD_FILE_DATA							0x0A		//send file data in packet
#define CMD_FW_UPDATE							0x0B		//start firmware update
#define	CMD_PREPARE_CALC					0x0C		//prepare calc voltage
#define CMD_START_CALC						0x0D		//start calc voltage
#define CMD_CLEAR_PERM_ERR				0x0E		//clear perm error status
#define CMD_TIME_SYNC							0x0F
#define CMD_RELAY_OP							0x10
#define CMD_PTCT_SET							0x11
#define CMD_I0CT_SET							0x12
#define CMD_VSET_SET							0x13
#define CMD_SET_FMD5							0x14
#define CMD_GET_MD5_RESULT				0x15
#define CMD_CANCFG_FILE_DATA			0x16
#define CMD_ACK										0x80

extern unsigned char md5sum2[];

void xfer_cmd(void)
{
	rt_pin_write(DC, PIN_LOW);
	rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_LOW);
	rt_thread_mdelay(1);
	spi_dma_tx_end = 0;
	HAL_SPI_Transmit_DMA(&hspi4, cmdbuf, 32);
	while(spi_dma_tx_end == 0)
		rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_HIGH);
	rt_thread_mdelay(1);
	
	rt_pin_write(DC, PIN_HIGH);
	rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_LOW);
	rt_thread_mdelay(1);
	spi_dma_rx_end = 0;
	HAL_SPI_Receive_DMA(&hspi4, cmdackbuf, 8);
	while(spi_dma_rx_end == 0)
		rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_HIGH);
	rt_thread_mdelay(1);
}

void recv_data(uint8_t * data, uint16_t len)
{
	rt_pin_write(DC, PIN_HIGH);
	rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_LOW);
	rt_thread_mdelay(1);
	spi_dma_rx_end = 0;
	HAL_SPI_Receive_DMA(&hspi4, data, len);
	while(spi_dma_rx_end == 0)
		rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_HIGH);
	rt_thread_mdelay(1);
}

void send_data(uint8_t * data, uint16_t len)
{
	rt_pin_write(DC, PIN_HIGH);
	rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_LOW);
	rt_thread_mdelay(1);
	spi_dma_tx_end = 0;
	HAL_SPI_Transmit_DMA(&hspi4, data, len);
	while(spi_dma_tx_end == 0)
		rt_thread_mdelay(1);
	rt_pin_write(SPICS, PIN_HIGH);
	rt_thread_mdelay(1);
}

void set_file_md5(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_SET_FMD5;
	cmdbuf[3] = 0x00;
	memcpy(&cmdbuf[4], md5sum2, 16);
	crc = calc_crc(cmdbuf, 20);
	cmdbuf[20] = crc>>8;
	cmdbuf[21] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			set_file_md5_req = 0;
			rt_thread_mdelay(10);
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void get_realtime_wave_data(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_REALTIME_WAVE_GET;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			recv_data((uint8_t *)wavebuf, 384*6+4*8);
			memcpy(&rms_result_ua, &wavebuf[384*6], 4);
			memcpy(&rms_result_ub, &wavebuf[384*6+4], 4);
			memcpy(&rms_result_uc, &wavebuf[384*6+4*2], 4);
			memcpy(&rms_result_u0, &wavebuf[384*6+4*3], 4);
			memcpy(&rms_result_ia, &wavebuf[384*6+4*4], 4);
			memcpy(&rms_result_ib, &wavebuf[384*6+4*5], 4);
			memcpy(&rms_result_ic, &wavebuf[384*6+4*6], 4);
			memcpy(&rms_result_i0, &wavebuf[384*6+4*7], 4);
			realtime_info.ua = rms_result_ua;
			realtime_info.ub = rms_result_ub;
			realtime_info.uc = rms_result_uc;
			realtime_info.u0 = rms_result_u0;
			realtime_info.ia = rms_result_ia;
			realtime_info.ib = rms_result_ib;
			realtime_info.ic = rms_result_ic;
			realtime_info.i0 = rms_result_i0;
			disp_wave_update = 1;
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void get_error_wave_data(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_ERROR_DATA_GET;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			recv_data((uint8_t *)error_wavebuf, 384*7);
			
			error_wave_update = 1;
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void get_staus(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_STATUS_GET;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			recv_data((uint8_t *)&calc_status_buf, sizeof(CALC_STATUS));
			if(0 != memcmp((uint8_t*)&calc_status_buf, (uint8_t*)&calc_status, sizeof(CALC_STATUS)))
			{
				if(0 != memcmp((uint8_t*)&calc_status_buf.clients_mask,(uint8_t*)&calc_status.clients_mask, sizeof(calc_status.clients_mask)))
					disp_node_page_update = 1;
				memcpy((uint8_t*)&calc_status, (uint8_t*)&calc_status_buf, sizeof(CALC_STATUS));
				calc_status_update = 1;
			}
			if(error_wave_update == 1)
			{
				if(calc_status.error_slow == 0)
				{
					gnd_error_info.ua = calc_status.error_ua*settings.pt/1000.0f;
					gnd_error_info.ub = calc_status.error_ub*settings.pt/1000.0f;
					gnd_error_info.uc = calc_status.error_uc*settings.pt/1000.0f;
					gnd_error_info.u0 = calc_status.error_u0;
					gnd_error_info.line = calc_status.error_line;
					gnd_error_info.id1 = calc_status.error_id_1;
					gnd_error_info.id2 = calc_status.error_id_2;
					gnd_error_info.id3 = calc_status.error_id_3;
					gnd_error_info.rms1 = calc_status.rms_1*settings.i0_ct;
					gnd_error_info.rms2 = calc_status.rms_2*settings.i0_ct;
					gnd_error_info.rms3 = calc_status.rms_3*settings.i0_ct;
					gnd_error_info.type = calc_status.error_type;
					gnd_error_info.ts = calc_status.error_ts;
					memset(ucSCoilBuf, 0x00, S_COIL_NCOILS/8);
					ucSCoilBuf[0] |= (1<<0);
					if(gnd_error_info.line == 1)
					{
						ucSCoilBuf[0] |= (1<<1);
					}
					else if(gnd_error_info.line == 2)
					{
						ucSCoilBuf[0] |= (1<<2);
					}
					else if(gnd_error_info.line == 3)
					{
						ucSCoilBuf[0] |= (1<<3);
					}
					if(gnd_error_info.type == 2)
					{
						ucSCoilBuf[0] |= (1<<4);
					}
					
					uint16_t idpos = 0;
					for(int i=0;i<client_nodes_cnt;i++)
					{
						if(client_nodes[i].id == gnd_error_info.id1)
						{
							idpos = i;
							break;
						}
					}
					uint16_t id_idx = idpos/8 + 1;
					uint16_t id_mask_idx = idpos%8;
					ucSCoilBuf[id_idx] |= (1<<id_mask_idx);
				}
				else if(calc_status.error_slow == VOLTAGE_OVER)
				{
					over_error_info.ua = calc_status.error_ua*settings.pt/1000.0f;
					over_error_info.ub = calc_status.error_ub*settings.pt/1000.0f;
					over_error_info.uc = calc_status.error_uc*settings.pt/1000.0f;
					over_error_info.u0 = calc_status.error_u0;
					over_error_info.ts = calc_status.error_ts;
				}
				else if(calc_status.error_slow == VOLTAGE_DROP)
				{
					drop_error_info.ua = calc_status.error_ua*settings.pt/1000.0f;
					drop_error_info.ub = calc_status.error_ub*settings.pt/1000.0f;
					drop_error_info.uc = calc_status.error_uc*settings.pt/1000.0f;
					drop_error_info.u0 = calc_status.error_u0;
					drop_error_info.ts = calc_status.error_ts;
				}
				else if(calc_status.error_slow == PT_ERROR)
				{
					pt_error_info.ua = calc_status.error_ua*settings.pt/1000.0f;
					pt_error_info.ub = calc_status.error_ub*settings.pt/1000.0f;
					pt_error_info.uc = calc_status.error_uc*settings.pt/1000.0f;
					pt_error_info.u0 = calc_status.error_u0;
					pt_error_info.ts = calc_status.error_ts;
				}
				else if(calc_status.error_slow == SHORT_ERROR)
				{
					short_error_info.ua = calc_status.error_ua*settings.pt/1000.0f;
					short_error_info.ub = calc_status.error_ub*settings.pt/1000.0f;
					short_error_info.uc = calc_status.error_uc*settings.pt/1000.0f;
					short_error_info.u0 = calc_status.error_u0;
					short_error_info.ts = calc_status.error_ts;
				}
				calc_status_ready = 1;
			}
			else
			{
				calc_status_ready = 0;
			}
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void time_sync(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_TIME_SYNC;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			time_t ts = (time_t)0;
			get_timestamp(&ts);
			send_data((uint8_t *)&ts, sizeof(ts));
			time_sync_req = 0;
			rt_thread_mdelay(1000);
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

uint8_t settings_buf[32];
volatile uint8_t save_ptct_req = 0;
volatile uint8_t save_i0ct_req = 0;
volatile uint8_t save_vset_req = 0;

extern SETTINGS settings;



void check_update_md5(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_GET_MD5_RESULT;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x81)
		{
			rt_thread_mdelay(10);
			check_update_md5_req = 0;
			md5_result = 1;
			md5_result_ready = 1;
			update_start = 1;
		}
		else if(cmdackbuf[4] == 0x01)
		{
			check_update_md5_req = 0;
			md5_result = 0;
			md5_result_ready = 1;
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void ptct_sync(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_PTCT_SET;
	cmdbuf[3] = 0x00;
	memcpy(&cmdbuf[4], &settings.pt, 2);
	memcpy(&cmdbuf[6], &settings.ct, 2);
	memcpy(&cmdbuf[8], &settings.i0_limt, 4);
	memcpy(&cmdbuf[12], &settings.i0_ct, 2);
	crc = calc_crc(cmdbuf, 0+14);
	cmdbuf[14] = crc>>8;
	cmdbuf[15] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			save_ptct_req = 0;
			rt_thread_mdelay(10);
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void vset_sync(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_VSET_SET;
	cmdbuf[3] = 0x00;
	cmdbuf[4] = settings.v_over;
	cmdbuf[5] = settings.v_drop;
	cmdbuf[6] = settings.u0_limt;
	crc = calc_crc(cmdbuf, 7);
	cmdbuf[7] = crc>>8;
	cmdbuf[8] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			save_vset_req = 0;
			rt_thread_mdelay(10);
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}


void calc_prepare(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_PREPARE_CALC;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		cmd_calc_prepare_req = 0;
	}
	else
	{
		m2m_error_cnt++;
	}
}
void calc_done(uint8_t vc)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_START_CALC;
	cmdbuf[3] = vc;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(vc == 1)
			cmd_calc_v_done_req = 0;
		else if(vc == 2)
			cmd_calc_c_done_req = 0;
	}
	else
	{
		m2m_error_cnt++;
	}
}
void clear_perm_error(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_CLEAR_PERM_ERR;
	cmdbuf[3] = 0x00;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		perm_error_clr_req = 0;
	}
	else
	{
		m2m_error_cnt++;
	}
}
void relay_op(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_RELAY_OP;
	cmdbuf[3] = relay_op_req;
	crc = calc_crc(cmdbuf, 0+4);
	cmdbuf[4] = crc>>8;
	cmdbuf[5] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		relay_op_req = 0;
	}
	else
	{
		m2m_error_cnt++;
	}
}
uint16_t packet_cnt = 0;
uint16_t packet_len = 0;
uint16_t packet_idx = 0;
volatile uint8_t packet_next = 0;
void send_file_packet(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_FILE_DATA;
	cmdbuf[3] = packet_idx>>4;
	cmdbuf[4] = (packet_idx<<4)|(packet_cnt>>8);
	cmdbuf[5] = packet_cnt;
	cmdbuf[6] = packet_len>>8;
	cmdbuf[7] = packet_len;
	crc = calc_crc(cmdbuf, 8);
	cmdbuf[8] = crc>>8;
	cmdbuf[9] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			send_data(file_buf, 4096);
			packet_next = 1;
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

void send_cancfg_file_packet(void)
{
	uint16_t crc,crc_recv;
	cmdbuf[0] = 0xFF;
	cmdbuf[1] = 0x55;
	cmdbuf[2] = CMD_CANCFG_FILE_DATA;
	cmdbuf[3] = packet_idx>>4;
	cmdbuf[4] = (packet_idx<<4)|(packet_cnt>>8);
	cmdbuf[5] = packet_cnt;
	cmdbuf[6] = packet_len>>8;
	cmdbuf[7] = packet_len;
	crc = calc_crc(cmdbuf, 8);
	cmdbuf[8] = crc>>8;
	cmdbuf[9] = crc;
	xfer_cmd();
	crc = calc_crc(cmdackbuf, 5);
	crc_recv = (cmdackbuf[5]<<8)|cmdackbuf[6];
	if((cmdackbuf[0] == 0xFF)&&(cmdackbuf[1] == 0x55)&&(crc == crc_recv))
	{
		if(cmdackbuf[4] == 0x01)
		{
			send_data(file_buf, 4096);
			packet_next = 1;
		}
	}
	else
	{
		m2m_error_cnt++;
	}
}

/**
  * @brief SPI4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI4_Init(void)
{

  /* USER CODE BEGIN SPI4_Init 0 */
	__HAL_RCC_DMA2_CLK_ENABLE();
  /* USER CODE END SPI4_Init 0 */

  /* USER CODE BEGIN SPI4_Init 1 */

  /* USER CODE END SPI4_Init 1 */
  /* SPI4 parameter configuration*/
  hspi4.Instance = SPI4;
  hspi4.Init.Mode = SPI_MODE_MASTER;
  hspi4.Init.Direction = SPI_DIRECTION_2LINES;
  hspi4.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi4.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi4.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi4.Init.NSS = SPI_NSS_SOFT;
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
	
	/* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

  /* USER CODE END SPI4_Init 2 */

}
uint8_t tab_idx;
extern FILE * firmwaref;
static void m2m_thread_entry(void *parameter)
{
	MX_SPI4_Init();
	calc_status.work_status = 0xFF;
	uint16_t recv_idx = 0;
	int8_t rx_wave_data[6];
	
	rt_pin_write(DC, PIN_HIGH);
	rt_pin_mode(DC, PIN_MODE_OUTPUT);
	
	rt_pin_write(RST, PIN_HIGH);
	rt_pin_mode(RST, PIN_MODE_OUTPUT);
	
	rt_pin_write(SPICS, PIN_HIGH);
	rt_pin_mode(SPICS, PIN_MODE_OUTPUT);
	
	rt_thread_mdelay(1000);
	rt_pin_write(RST, PIN_LOW);
	rt_thread_mdelay(100);
	rt_pin_write(RST, PIN_HIGH);
	
	while(1)
	{
		rt_thread_mdelay(10);
		if(ui_loaded == 1)
		{
			tab_idx = lv_tabview_get_tab_act(ui_TabView1);
			if(sync_update_file == 1)
			{
				if(packet_next == 1)
				{
					packet_next = 0;
					if(packet_idx == (packet_cnt-1))
					{
						fclose(firmwaref);
						rt_kprintf("file sync finshed\n");
						sync_update_file = 0;
						check_update_md5_req = 1;
					}
					else
					{
						packet_len = fread(file_buf, 1, 4096, firmwaref);
						packet_idx++;
					}
				}
				else
				{
					send_file_packet();
				}
			}
			if(sync_cancfg_file == 1)
			{
				if(packet_next == 1)
				{
					packet_next = 0;
					if(packet_idx == (packet_cnt-1))
					{
						fclose(firmwaref);
						rt_kprintf("file sync finshed\n");
						sync_cancfg_file = 0;
						rt_thread_mdelay(1000);
						rt_hw_cpu_reset();
					}
					else
					{
						packet_len = fread(file_buf, 1, 4096, firmwaref);
						packet_idx++;
					}
				}
				else
				{
					send_cancfg_file_packet();
				}
			}
			if(display_data_disable != 1)
			{
				get_realtime_wave_data();
				rt_thread_mdelay(100);
			}
			get_error_wave_data();
			rt_thread_mdelay(100);
			get_staus();
			rt_thread_mdelay(100);
			if(time_sync_req == 1)
			{
				time_sync();
				rt_thread_mdelay(10);
			}
			if(perm_error_clr_req == 1)
			{
				clear_perm_error();
			}
			if(cmd_calc_prepare_req == 1)
			{
				calc_prepare();
				rt_thread_mdelay(10);
			}
			if(cmd_calc_v_done_req == 1)
			{
				calc_done(1);
				rt_thread_mdelay(10);
			}
			if(cmd_calc_c_done_req == 1)
			{
				calc_done(2);
				rt_thread_mdelay(10);
			}
			if(relay_op_req != 0)
			{
				relay_op();
				rt_thread_mdelay(10);
			}
			if(save_ptct_req == 1)
			{
				ptct_sync();
				rt_thread_mdelay(10);
			}
			if(save_vset_req == 1)
			{
				vset_sync();
				rt_thread_mdelay(10);
			}
			if(set_file_md5_req == 1)
			{
				set_file_md5();
				rt_thread_mdelay(10);
			}
			if(check_update_md5_req == 1)
			{
				check_update_md5();
				rt_thread_mdelay(10);
			}
			
			if(m2m_error_cnt > 100)
			{
				rt_pin_write(RST, PIN_LOW);
				rt_thread_mdelay(100);
				rt_pin_write(RST, PIN_HIGH);
				rt_thread_mdelay(100);
			}
			
		}
	}
}

void M2M_HAL_SPI_RxCpltCallback(void)
{
	spi_dma_rx_end = 1;
}
void M2M_HAL_SPI_TxCpltCallback(void)
{
	spi_dma_tx_end = 1;
}

void DMA2_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream1_IRQn 0 */
	rt_interrupt_enter();
  /* USER CODE END DMA2_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi4_tx);
  /* USER CODE BEGIN DMA2_Stream1_IRQn 1 */
	rt_interrupt_leave();
  /* USER CODE END DMA2_Stream1_IRQn 1 */
}

void DMA2_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream3_IRQn 0 */
	rt_interrupt_enter();
  /* USER CODE END DMA2_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_spi4_rx);
  /* USER CODE BEGIN DMA2_Stream3_IRQn 1 */
	rt_interrupt_leave();
  /* USER CODE END DMA2_Stream3_IRQn 1 */
}

void SPI4_IRQHandler(void)
{
  /* USER CODE BEGIN SPI4_IRQn 0 */
	rt_interrupt_enter();
  /* USER CODE END SPI4_IRQn 0 */
  HAL_SPI_IRQHandler(&hspi4);
  /* USER CODE BEGIN SPI4_IRQn 1 */
	rt_interrupt_leave();
  /* USER CODE END SPI4_IRQn 1 */
}

static int m2m_thread_init(void)
{
	rt_err_t err;

	err = rt_thread_init(&m2m_thread, "m2m", m2m_thread_entry, RT_NULL,
			 &m2m_thread_stack[0], sizeof(m2m_thread_stack), M2M_THREAD_PRIO, 10);
	if(err != RT_EOK)
	{
		rt_kprintf("Failed to create MISC thread");
		return -1;
	}
	rt_thread_startup(&m2m_thread);

	return 0;
}
INIT_ENV_EXPORT(m2m_thread_init);
