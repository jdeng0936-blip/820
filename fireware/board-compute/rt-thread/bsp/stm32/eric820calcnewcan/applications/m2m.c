#include "main.h"

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>

#define WAVE_THREAD_STACK_SIZE 512
#define WAVE_THREAD_PRIO 27
static rt_uint8_t wave_thread_stack[WAVE_THREAD_STACK_SIZE];
static struct rt_thread wave_thread;
#define M2M_THREAD_STACK_SIZE 512
#define M2M_THREAD_PRIO (RT_THREAD_PRIORITY_MAX*2/3)
static rt_uint8_t m2m_thread_stack[M2M_THREAD_STACK_SIZE];
static struct rt_thread m2m_thread;
extern rt_mq_t wave_data_mq;
#define DC			GET_PIN(F, 6)
#define RST			GET_PIN(F, 7)
#define SPICS		GET_PIN(E, 4)

extern SPI_HandleTypeDef hspi4;
extern volatile uint16_t error_can_id_1;
extern volatile uint16_t error_can_id_2;
extern volatile uint16_t error_can_id_3;
extern volatile uint8_t error_data_ready;
extern volatile uint8_t settings_changed;
extern volatile uint8_t error_line;
extern volatile uint8_t gnd_relay_work;
extern volatile uint8_t backup_data_start;
extern volatile uint8_t backup_data_end;
extern uint16_t backup_data_w_idx;
extern uint16_t backup_data_w_cnt;
extern volatile uint8_t relay_work_finshed;
volatile uint8_t gnd_err_clr = 0;
float backup_ratio_ua = 0.0f;
float backup_ratio_ub = 0.0f;
float backup_ratio_uc = 0.0f;
float backup_ratio_u0 = 0.0f;

float backup_ratio_ia = 0.0f;
float backup_ratio_ib = 0.0f;
float backup_ratio_ic = 0.0f;
float backup_ratio_i0 = 0.0f;
volatile uint8_t last_cmd = 0;

#define	PERI_GND								0x01
#define	PERM_GND								0x02

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

CALC_STATUS calc_status;

uint8_t * wavebuf;//[768*6*2] = {0};

uint8_t cmdbuf[32];
uint8_t cmdackbuf[8];
uint8_t sendbuf1[384*6+4*8];
uint8_t sendbuf2[384*6+4*8];

extern int8_t error_buf[];

extern rt_sem_t spi_start_sem;

volatile uint8_t spi_dma_rx_end = 1;
volatile uint8_t spi_dma_tx_end = 1;

#define M2M_NONE		0
#define M2M_READ		1
#define M2M_WRITE		2

uint8_t m2m_wr = M2M_NONE;
uint16_t m2m_len = 0;
uint8_t * m2m_data;
uint8_t m2m_ack = 0;

volatile uint8_t wave_ava1 = 0;
volatile uint8_t wave_ava2 = 0;

volatile uint8_t wave_one1 = 0;
volatile uint8_t wave_one2 = 0;

volatile uint16_t wave_cnt = 0;

volatile uint16_t wave_w_idx = 0;

extern int16_t rms_avg_ua;
extern int16_t rms_avg_ub;
extern int16_t rms_avg_uc;
extern int16_t rms_avg_u0;

extern int16_t rms_avg_ia;
extern int16_t rms_avg_ib;
extern int16_t rms_avg_ic;
extern int16_t rms_avg_i0;

extern volatile uint8_t slow_error_ack;

volatile uint8_t md5_result = 0;
volatile uint8_t md5_result_ready = 0;

unsigned char md5sum2[16];

time_t ts = (time_t)0;

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

void make_ack_buf(uint8_t ack)
{
	uint16_t crc;
	cmdackbuf[0] = 0xFF;
	cmdackbuf[1] = 0x55;
	cmdackbuf[2] = CMD_ACK;
	cmdackbuf[3] = 0x01;
	cmdackbuf[4] = ack;
	crc = calc_crc(cmdackbuf, 5);
	cmdackbuf[5] = crc>>8;
	cmdackbuf[6] = crc;
}
volatile uint8_t file_write_busy = 0;
volatile uint8_t cfgfile_write_busy = 0;
uint8_t file_buf[4096];
uint16_t packet_cnt = 0;
uint16_t packet_len = 0;
uint16_t packet_idx = 0;
uint8_t settings_buf[16];
static void m2m_thread_entry(void *parameter)
{
	rt_thread_mdelay(3000);
	rt_sem_control(spi_start_sem, RT_IPC_CMD_RESET, RT_NULL);
	while(1)
	{
		if(rt_sem_take(spi_start_sem, RT_WAITING_FOREVER) == RT_EOK)
		{
			if(HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_6) == GPIO_PIN_RESET)
			{
				spi_dma_rx_end = 0;
				HAL_SPI_Receive_DMA(&hspi4, cmdbuf, 32);
				while(spi_dma_rx_end == 0)
					rt_thread_mdelay(1);
				uint16_t crc, crc_recv;
				if((cmdbuf[2] == CMD_FILE_DATA)||(cmdbuf[2] == CMD_CANCFG_FILE_DATA))
				{
					crc = calc_crc(cmdbuf, 8);
					crc_recv = (cmdbuf[8]<<8)|cmdbuf[9];
				}
				else if(cmdbuf[2] == CMD_PTCT_SET)
				{
					crc = calc_crc(cmdbuf, 14);
					crc_recv = (cmdbuf[14]<<8)|cmdbuf[15];
				}
				else if(cmdbuf[2] == CMD_VSET_SET)
				{
					crc = calc_crc(cmdbuf, 7);
					crc_recv = (cmdbuf[7]<<8)|cmdbuf[8];
				}
				else if(cmdbuf[2] == CMD_SET_FMD5)
				{
					crc = calc_crc(cmdbuf, 20);
					crc_recv = (cmdbuf[20]<<8)|cmdbuf[21];
				}
				else
				{
					crc = calc_crc(cmdbuf, 4);
					crc_recv = (cmdbuf[4]<<8)|cmdbuf[5];
				}

				if((cmdbuf[0] == 0xFF)&&(cmdbuf[1] == 0x55)&&(crc == crc_recv))
				{
					last_cmd = cmdbuf[2];
					switch(cmdbuf[2])
					{
						case CMD_FILE_DATA:
						{
							m2m_ack = 1;
							if(file_write_busy == 1)
							{
								make_ack_buf(0);
								m2m_wr = M2M_NONE;
							}
							else
							{
								packet_len = (cmdbuf[6]<<8)|cmdbuf[7];
								make_ack_buf(1);
								m2m_wr = M2M_WRITE;
								m2m_data = file_buf;
								m2m_len = packet_len;
								packet_idx = (cmdbuf[3]<<4)|(cmdbuf[4]>>4);
								packet_cnt = ((cmdbuf[4]&0x0F)<<8)|cmdbuf[5];
							}
						}
						break;
						case CMD_CANCFG_FILE_DATA:
						{
							m2m_ack = 1;
							if(cfgfile_write_busy == 1)
							{
								make_ack_buf(0);
								m2m_wr = M2M_NONE;
							}
							else
							{
								packet_len = (cmdbuf[6]<<8)|cmdbuf[7];
								make_ack_buf(1);
								m2m_wr = M2M_WRITE;
								m2m_data = file_buf;
								m2m_len = packet_len;
								packet_idx = (cmdbuf[3]<<4)|(cmdbuf[4]>>4);
								packet_cnt = ((cmdbuf[4]&0x0F)<<8)|cmdbuf[5];
							}
						}
						break;
						case CMD_REALTIME_WAVE_GET:
						{
							m2m_ack = 1;
							
							if(wave_ava1==1)
							{
								wave_ava1 = 0;
								make_ack_buf(1);
								m2m_wr = M2M_READ;
								m2m_len = 384*6+4*8;
								m2m_data = sendbuf1;
							}
							else if(wave_ava2==1)
							{
								wave_ava2 = 0;
								make_ack_buf(1);
								m2m_wr = M2M_READ;
								m2m_len = 384*6+4*8;
								m2m_data = sendbuf2;
							}
							else
							{
								make_ack_buf(0);
								m2m_wr = M2M_NONE;
							}
						}
						break;
						case CMD_PREPARE_CALC:
						{
							backup_ratio_ua = calc_status.settings.rms_ratio_ua;
							backup_ratio_ub = calc_status.settings.rms_ratio_ub;
							backup_ratio_uc = calc_status.settings.rms_ratio_uc;
							backup_ratio_u0 = calc_status.settings.rms_ratio_u0;
							backup_ratio_ia = calc_status.settings.rms_ratio_ia;
							backup_ratio_ib = calc_status.settings.rms_ratio_ib;
							backup_ratio_ic = calc_status.settings.rms_ratio_ic;
							backup_ratio_i0 = calc_status.settings.rms_ratio_i0;
							
							calc_status.settings.rms_ratio_ua = 110.0f;
							calc_status.settings.rms_ratio_ub = 110.0f;
							calc_status.settings.rms_ratio_uc = 110.0f;
							calc_status.settings.rms_ratio_u0 = 110.0f;
							calc_status.settings.rms_ratio_ia = 2000.0f;
							calc_status.settings.rms_ratio_ib = 2000.0f;
							calc_status.settings.rms_ratio_ic = 2000.0f;
							calc_status.settings.rms_ratio_i0 = 4000.0f;
							make_ack_buf(1);
							m2m_ack = 1;
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_START_CALC:
						{
							if(cmdbuf[3] == 0x01)
							{
								calc_status.settings.rms_ratio_ua = (float)rms_avg_ua / 100.0f;
								calc_status.settings.rms_ratio_ub = (float)rms_avg_ub / 100.0f;
								calc_status.settings.rms_ratio_uc = (float)rms_avg_uc / 100.0f;
								calc_status.settings.rms_ratio_u0 = (float)rms_avg_u0 / 100.0f;
								
								backup_ratio_ua = calc_status.settings.rms_ratio_ua;
								backup_ratio_ub = calc_status.settings.rms_ratio_ub;
								backup_ratio_uc = calc_status.settings.rms_ratio_uc;
								backup_ratio_u0 = calc_status.settings.rms_ratio_u0;
								
								calc_status.settings.rms_ratio_ia = backup_ratio_ia;
								calc_status.settings.rms_ratio_ib = backup_ratio_ib;
								calc_status.settings.rms_ratio_ic = backup_ratio_ic;
								calc_status.settings.rms_ratio_i0 = backup_ratio_i0;
							}
							if(cmdbuf[3] == 0x02)
							{
								calc_status.settings.rms_ratio_ua = backup_ratio_ua;
								calc_status.settings.rms_ratio_ub = backup_ratio_ub;
								calc_status.settings.rms_ratio_uc = backup_ratio_uc;
								calc_status.settings.rms_ratio_u0 = backup_ratio_u0;
								
								calc_status.settings.rms_ratio_ia = (float)rms_avg_ia / 5.0f;
								calc_status.settings.rms_ratio_ib = (float)rms_avg_ib / 5.0f;
								calc_status.settings.rms_ratio_ic = (float)rms_avg_ic / 5.0f;
								calc_status.settings.rms_ratio_i0 = (float)rms_avg_i0 / 5.0f;
								
								backup_ratio_ia = calc_status.settings.rms_ratio_ia;
								backup_ratio_ib = calc_status.settings.rms_ratio_ib;
								backup_ratio_ic = calc_status.settings.rms_ratio_ic;
								backup_ratio_i0 = calc_status.settings.rms_ratio_i0;
							}
							settings_changed = 1;
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_CLEAR_PERM_ERR:
						{
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
							HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
							HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
							HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
							HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
							gnd_err_clr = 1;
							//rt_thread_mdelay(1000);
						}
						break;
						case CMD_STATUS_GET:
						{
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_READ;
							m2m_len = sizeof(CALC_STATUS);
							m2m_data = (uint8_t*)&calc_status;
						}
						break;
						case CMD_PTCT_SET:
						{
							memcpy(&calc_status.settings.pt, &cmdbuf[4], 2);
							memcpy(&calc_status.settings.ct, &cmdbuf[6], 2);
							memcpy(&calc_status.settings.i0_limt, &cmdbuf[8], 4);
							memcpy(&calc_status.settings.i0_ct, &cmdbuf[12], 2);
							settings_changed = 1;
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_VSET_SET:
						{
							calc_status.settings.v_over = cmdbuf[4];
							calc_status.settings.v_drop = cmdbuf[5];
							calc_status.settings.u0_limt = cmdbuf[6];
							settings_changed = 1;
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_RELAY_OP:
						{
							switch(cmdbuf[3])
							{
								case 0x10:
									HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
									break;
								case 0x11:
									HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_SET);
									break;
								case 0x20:
									HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
									break;
								case 0x21:
									HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_SET);
									break;
								case 0x30:
									HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
									break;
								case 0x31:
									HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_SET);
									break;
								case 0x40:
									HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
									break;
								case 0x41:
									HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
									break;
								case 0x50:
									HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, GPIO_PIN_RESET);
									break;
								case 0x51:
									HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, GPIO_PIN_SET);
									break;
							}
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_ERROR_DATA_GET:
						{
							m2m_ack = 1;
							if((error_data_ready == 1)&&(relay_work_finshed == 1))
							{
								if(fast_error_line != 0)
								{
									calc_status.error_slow = 0;
									calc_status.error_line = fast_error_line;
									calc_status.error_id_1 = error_can_id_1;
									calc_status.error_id_2 = error_can_id_2;
									calc_status.error_id_3 = error_can_id_3;
									if(gnd_relay_work == 0)
									{
										calc_status.error_type = PERI_GND;
										fast_error = 0;
									}
									else
										calc_status.error_type = PERM_GND;
									slow_gnd_line = 0;
									fast_error_line = 0;
								}
								else
								{
									calc_status.error_line = 0;
									calc_status.error_slow = slow_error_type;
									slow_error_ack = 1;
								}
								error_data_ready = 0;
								backup_data_start = 0;
								backup_data_end = 0;
								backup_data_w_cnt = 0;
								backup_data_w_idx = 0;
								
								make_ack_buf(1);
								m2m_wr = M2M_READ;
								m2m_len = 384*7;
								m2m_data = (uint8_t *)error_buf;
							}
							else
							{
								make_ack_buf(0);
								m2m_wr = M2M_NONE;
							}
						}
						break;
						case CMD_SET_FMD5:
						{
							m2m_ack = 1;
							memcpy(md5sum2, &cmdbuf[4], 16);
							make_ack_buf(1);
							m2m_wr = M2M_NONE;
						}
						break;
						case CMD_GET_MD5_RESULT:
						{
							m2m_ack = 1;
							if(md5_result_ready == 1)
							{
								make_ack_buf(md5_result | 1);
								m2m_wr = M2M_NONE;
							}
							else
							{
								make_ack_buf(0);
								m2m_wr = M2M_NONE;
							}
						}
						break;
						case CMD_TIME_SYNC:
						{
							m2m_ack = 1;
							make_ack_buf(1);
							m2m_wr = M2M_WRITE;
							m2m_data = (uint8_t *)&ts;
							m2m_len = sizeof(ts);
						}
						break;
					}
				}
			}
			else
			{
				if(m2m_ack == 1)
				{
					m2m_ack = 0;
					spi_dma_tx_end = 0;
					HAL_SPI_Transmit_DMA(&hspi4, cmdackbuf, 8);
					while(spi_dma_tx_end == 0)
						rt_thread_mdelay(1);
				}
				else if(m2m_wr == M2M_READ)
				{
					spi_dma_tx_end = 0;
					HAL_SPI_Transmit_DMA(&hspi4, m2m_data, m2m_len);
					while(spi_dma_tx_end == 0)
						rt_thread_mdelay(1);
				}
				else if(m2m_wr == M2M_WRITE)
				{
					spi_dma_rx_end = 0;
					HAL_SPI_Receive_DMA(&hspi4, m2m_data, m2m_len);
					while(spi_dma_rx_end == 0)
						rt_thread_mdelay(1);
					if(last_cmd == CMD_TIME_SYNC)
					{
						set_timestamp(ts);
					}
					else if(last_cmd == CMD_FILE_DATA)
					{
						file_write_busy = 1;
					}
					else if(last_cmd == CMD_CANCFG_FILE_DATA)
					{
						cfgfile_write_busy = 1;
					}
				}
			}
			rt_sem_control(spi_start_sem, RT_IPC_CMD_RESET, RT_NULL);
		}
	}
}

static void wave_thread_entry(void *parameter)
{
	int8_t rx_wave_data[6];
	rt_thread_mdelay(3000);
	while(1)
	{
		rt_mq_recv(wave_data_mq, &rx_wave_data, 6, RT_WAITING_FOREVER);
		wavebuf[wave_w_idx*6] = rx_wave_data[0];
		wavebuf[wave_w_idx*6+1] = rx_wave_data[1];
		wavebuf[wave_w_idx*6+2] = rx_wave_data[2];
		wavebuf[wave_w_idx*6+3] = rx_wave_data[3];
		wavebuf[wave_w_idx*6+4] = rx_wave_data[4];
		wavebuf[wave_w_idx*6+5] = rx_wave_data[5];
		
		wave_w_idx++;
		if(wave_w_idx == 384)
		{
			//if(wave_one1 == 0)
			{
				memcpy(sendbuf1, &wavebuf[0], 384*6);
				//wave_one1 = 1;
			}
			wave_ava1 = 1;
		}
		else if(wave_w_idx == 768)
		{
			//if(wave_one2 == 0)
			{
				memcpy(sendbuf2, &wavebuf[384*6], 384*6);
				//wave_one2 = 1;
			}
			wave_ava2 = 1;
			wave_w_idx = 0;
		}
	}
}

void M2M_HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
	spi_dma_rx_end = 1;
}
void M2M_HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	spi_dma_tx_end = 1;
}

static int m2m_thread_init(void)
{
	rt_err_t err;
	
	wavebuf = rt_memheap_alloc(&system_heap, 384*6*2);
	
	err = rt_thread_init(&wave_thread, "wave", wave_thread_entry, RT_NULL,
			 &wave_thread_stack[0], sizeof(wave_thread_stack), WAVE_THREAD_PRIO, 10);
	if(err != RT_EOK)
	{
		rt_kprintf("Failed to create WAVE thread");
		return -1;
	}
	rt_thread_startup(&wave_thread);

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
