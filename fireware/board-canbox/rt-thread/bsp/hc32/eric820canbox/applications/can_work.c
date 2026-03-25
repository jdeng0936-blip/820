#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <stdlib.h>
#include "drv_can.h"
#include <string.h>
#include "drv_flash.h"

settings_t * settings = (settings_t*)SETTINGS_ADDR;

settings_t settings_buf;

#define CAN_TX_THREAD_STACK_SIZE	512
#define CAN_TX_THREAD_PRIORITY		30
#define CAN_TX_THREAD_SLICE_MS		10

#define CAN_RX_THREAD_STACK_SIZE	512
#define CAN_RX_THREAD_PRIORITY		29
#define CAN_RX_THREAD_SLICE_MS		10

#define CAN_CMD_BC					0x01			//广播指令 回复继电器状态
#define CAN_CMD_GET_VAL			0x02			//获取电流值
#define CAN_CMD_GET_WAVE		0x03			//获取波形
#define CAN_CMD_SET_RELAY		0x04			//操作继电器
#define CAN_CMD_SET_ID			0x05			//设置ID
#define CAN_CMD_CALC				0x06			//校准

#define RELAY_PIN GET_PIN(B, 14)

volatile uint8_t can_online = 0;
uint8_t can_idle = 0;
rt_device_t can_dev = RT_NULL;
volatile uint8_t can_upload_wave_req = 0;

uint8_t can_tx_buffer[384];

uint16_t selfid = 0x400+20;
float calc_val = 0.0724637657f;

rt_mq_t rx_data_mq;
rt_mq_t tx_data_mq;

uint16_t calc_crc(uint8_t *data, uint16_t length)
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


rt_err_t can_rx_call(rt_device_t dev, rt_size_t size)
{
	struct rt_can_msg rxmsg = {0};
	rt_memset(&rxmsg, 0, sizeof(struct rt_can_msg));
	rxmsg.hdr_index = -1;
	rt_device_read(can_dev, 0, &rxmsg, sizeof(rxmsg));
	
	rt_mq_send(rx_data_mq, &rxmsg, sizeof(rxmsg));
	can_idle = 0;
	can_online = 1;
	return RT_EOK;
}

void _set_default_filter(void)
{
#ifdef RT_CAN_USING_HDR
    struct rt_can_filter_item can_items[3] =
    {
        {selfid, RT_CAN_STDID, RT_CAN_DTR, 1, 0x7ff, 5},                                                  /* std,match ID:selfid，hdr= 6，指定设置6号过滤表，接收自己ID的消息 */
        {0x300, RT_CAN_STDID, RT_CAN_DTR, 1, 0x7ff, 6},                                                  /* std,match ID:0x300，hdr= 7，指定设置7号过滤表，接收广播ID的消息 */
				{0x301, RT_CAN_STDID, RT_CAN_DTR, 1, 0x7ff, 7}                                                  /* std,match ID:0x300，hdr= 7，指定设置7号过滤表，接收鞋码与校准的消息 */
    };
    struct rt_can_filter_config cfg = {3, 1, can_items}; /* 一共有2个过滤表，1表示初始化过滤表控制块 */
    rt_err_t res;
    res = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &cfg);
    RT_ASSERT(res == RT_EOK);
#endif
}

static void can_rx_thread(void * param)
{
	uint16_t wave_crc;
	struct rt_can_msg rxmsg = {0};
	struct rt_can_msg txmsg = {0};
	
	while(1)
	{
		rt_mq_recv(rx_data_mq, &rxmsg, sizeof(rxmsg), RT_WAITING_FOREVER);
		/* 打印数据 ID 及内容 */
//		rt_kprintf("ID:%x Data:", rxmsg.id);
//		for (int i = 0; i < 8; i++)
//		{
//				rt_kprintf("%2x ", rxmsg.data[i]);
//		}
//		rt_kprintf("\n");
		switch(rxmsg.data[0])
		{
			case CAN_CMD_BC:
			{
				txmsg.data[0] = CAN_CMD_BC | 0x80;
				if(rt_pin_read(RELAY_PIN) == PIN_HIGH)
					txmsg.data[1] = 1;
				else
					txmsg.data[1] = 0;
				txmsg.id  = selfid;
        txmsg.ide = RT_CAN_STDID;
        txmsg.rtr = RT_CAN_DTR;
        txmsg.len = 8;
				rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
			}
			break;
			case CAN_CMD_GET_VAL:
			{
				can_upload_wave_req = 1;
				while(can_upload_wave_req == 1)
					rt_thread_yield();
				int8_t max = 0,min = 0;
				for(int i=0;i<384;i++)
				{
					if(max<(int8_t)can_tx_buffer[i])
						max = (int8_t)can_tx_buffer[i];
					if(min>(int8_t)can_tx_buffer[i])
						min = (int8_t)can_tx_buffer[i];
				}
				if(max<-min)
					max = -min;
				txmsg.data[0] = CAN_CMD_GET_VAL | 0x80;
				float val = settings->calc_val*max;
				if(val<0.2f)
					val = 0.0f;
				memcpy(&txmsg.data[1], &val, 4);
				txmsg.id  = selfid;
        txmsg.ide = RT_CAN_STDID;
        txmsg.rtr = RT_CAN_DTR;
        txmsg.len = 8;
				rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
			}
			break;
			case CAN_CMD_GET_WAVE:
			{
				uint16_t target_id = (rxmsg.data[2]<<8)|rxmsg.data[1];
				if(target_id == selfid)
				{
					txmsg.data[0] = CAN_CMD_GET_WAVE | 0x80;
					wave_crc = calc_crc(can_tx_buffer, 384);
					txmsg.data[1] = wave_crc>>8;
					txmsg.data[2] = wave_crc;
					txmsg.id  = selfid;
					txmsg.ide = RT_CAN_STDID;
					txmsg.rtr = RT_CAN_DTR;
					txmsg.len = 8;
					rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
					txmsg.id  = 0x7E9;
					txmsg.len = 8;
					for(int i=0;i<48;i++)
					{
						memcpy(txmsg.data, &can_tx_buffer[8*i], 8);
						rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
					}
				}
			}
			break;
			case CAN_CMD_SET_RELAY:
			{
				uint16_t target_id = (rxmsg.data[2]<<8)|rxmsg.data[1];
				if(target_id == selfid)
				{
					txmsg.data[0] = CAN_CMD_SET_RELAY | 0x80;
					txmsg.data[1] = rxmsg.data[1];
					txmsg.data[2] = rxmsg.data[2];
					txmsg.id  = selfid;
					txmsg.ide = RT_CAN_STDID;
					txmsg.rtr = RT_CAN_DTR;
					txmsg.len = 8;
					rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
				
					if(rxmsg.data[3] == 0)
					{
						//脉冲方式工作
						rt_kprintf("relay on\r");
						rt_pin_write(RELAY_PIN, PIN_HIGH);
						rt_thread_mdelay(rxmsg.data[2]*10);
						rt_pin_write(RELAY_PIN, PIN_LOW);
						rt_kprintf("relay off\r");
					}
					else
					{
						//电平方式工作
						if(rxmsg.data[4] == 1)
							rt_pin_write(RELAY_PIN, PIN_HIGH);
						else
							rt_pin_write(RELAY_PIN, PIN_LOW);
					}
				}
			}
			break;
			case CAN_CMD_SET_ID:
			{
				memcpy(&settings_buf, settings, sizeof(settings_buf));
				settings_buf.self_id = (rxmsg.data[1]<<8)|rxmsg.data[2];
				settings_buf.sign = 0xa55a55aa;
				hc32_flash_erase(SETTINGS_ADDR, 8192);
				hc32_flash_write(SETTINGS_ADDR, (uint8_t*)&settings_buf, sizeof(settings_buf));
				selfid = settings_buf.self_id;
				_set_default_filter();
				txmsg.data[0] = CAN_CMD_SET_ID | 0x80;
				txmsg.data[1] = rxmsg.data[1];
				txmsg.data[2] = rxmsg.data[2];
				txmsg.id  = selfid;
        txmsg.ide = RT_CAN_STDID;
        txmsg.rtr = RT_CAN_DTR;
        txmsg.len = 8;
				rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
				//rt_thread_mdelay(1000);
				//rt_hw_cpu_reset();
			}
			break;
			case CAN_CMD_CALC:
			{
				int8_t max = 0,min = 0;
				can_upload_wave_req = 1;
				while(can_upload_wave_req == 1)
					rt_thread_yield();
				for(int i=0;i<384;i++)
				{
					if(max<(int8_t)can_tx_buffer[i])
						max = (int8_t)can_tx_buffer[i];
					if(min>(int8_t)can_tx_buffer[i])
						min = (int8_t)can_tx_buffer[i];
				}
				if(max<-min)
					max = -min;
				memcpy(&settings_buf, settings, sizeof(settings_buf));
				settings_buf.calc_val = 5.0f/(float)max;
				settings_buf.sign = 0xa55a55aa;
				hc32_flash_erase(SETTINGS_ADDR, 8192);
				hc32_flash_write(SETTINGS_ADDR, (uint8_t*)&settings_buf, sizeof(settings_buf));
				txmsg.data[0] = CAN_CMD_CALC | 0x80;
				txmsg.id  = selfid;
        txmsg.ide = RT_CAN_STDID;
        txmsg.rtr = RT_CAN_DTR;
        txmsg.len = 8;
				rt_mq_send(tx_data_mq, &txmsg, sizeof(txmsg));
			}
			break;
		}
	}
}
static void can_tx_thread(void * param)
{
	struct rt_can_msg txmsg = {0};
	rt_size_t  size;
	while(1)
	{
		rt_mq_recv(tx_data_mq, &txmsg, sizeof(txmsg), RT_WAITING_FOREVER);
		size = rt_device_write(can_dev, 0, &txmsg, sizeof(txmsg));
		if (size == 0)
		{
				rt_kprintf("can dev write data failed!\n");
		}
		rt_kprintf("send msg ok! \n");
	}
}
int can_comm_init(void)
{
	rt_thread_t canrx_thread;
	rt_thread_t cantx_thread;
	
	rt_pin_write(RELAY_PIN, PIN_LOW);
	rt_pin_mode(RELAY_PIN, PIN_MODE_OUTPUT);
	
	rx_data_mq = rt_mq_create("canr_mq", sizeof(struct rt_can_msg), 16, RT_IPC_FLAG_FIFO);
	tx_data_mq = rt_mq_create("cant_mq", sizeof(struct rt_can_msg), 64, RT_IPC_FLAG_FIFO);
	
	can_dev = rt_device_find("can1");
	
	rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
	rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, (void *)CAN200kBaud);
	rt_device_control(can_dev, RT_CAN_CMD_SET_MODE, (void *)RT_CAN_MODE_NORMAL);
	
	if(settings->sign == 0xa55a55aa)
	{
		selfid = settings->self_id;
	}
//	else
//	{
//		selfid = 0x7ff;
//	}
	
	_set_default_filter();

	cantx_thread = rt_thread_create("cant_th", can_tx_thread, RT_NULL,
														CAN_TX_THREAD_STACK_SIZE,
														CAN_TX_THREAD_PRIORITY,
														CAN_TX_THREAD_SLICE_MS);
	rt_thread_startup(cantx_thread);
	
	canrx_thread = rt_thread_create("canr_th", can_rx_thread, RT_NULL,
														CAN_RX_THREAD_STACK_SIZE,
														CAN_RX_THREAD_PRIORITY,
														CAN_RX_THREAD_SLICE_MS);
	rt_thread_startup(canrx_thread);
	
	rt_device_set_rx_indicate(can_dev, can_rx_call);
	
	return 0;
}
INIT_APP_EXPORT(can_comm_init);

static int set_id(int argc, char **argv)
{
	/* 参数无输入或者输入错误按照默认值处理 */
	if (argc == 2)
	{
		uint16_t canid = atol(argv[1]);
		memcpy(&settings_buf, settings, sizeof(settings_buf));
		settings_buf.self_id = 0x400 + canid;
		settings_buf.sign = 0xa55a55aa;
		hc32_flash_erase(SETTINGS_ADDR, 8192);
		hc32_flash_write(SETTINGS_ADDR, (uint8_t*)&settings_buf, sizeof(settings_buf));
		rt_thread_mdelay(1000);
		rt_hw_cpu_reset();
	}
	return RT_EOK;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(set_id, set can id sample: set_id 401 );

static int set_calc(int argc, char **argv)
{
	/* 参数无输入或者输入错误按照默认值处理 */
	memcpy(&settings_buf, settings, sizeof(settings_buf));
	settings_buf.sign = 0xa55a55aa;
	if(argc == 2)
	{
		float cval = atof(argv[1]);
		settings_buf.calc_val = cval;
	}
	else
	{
		int8_t max = 0,min = 0;
		can_upload_wave_req = 1;
		while(can_upload_wave_req == 1)
			rt_thread_mdelay(10);
		for(int i=0;i<384;i++)
		{
			if(max<(int8_t)can_tx_buffer[i])
				max = (int8_t)can_tx_buffer[i];
			if(min>(int8_t)can_tx_buffer[i])
				min = (int8_t)can_tx_buffer[i];
		}
		if(max<-min)
			max = -min;
		settings_buf.calc_val = 5.0f/(float)max;
	}
	hc32_flash_erase(SETTINGS_ADDR, 8192);
	hc32_flash_write(SETTINGS_ADDR, (uint8_t*)&settings_buf, sizeof(settings_buf));
	rt_thread_mdelay(1000);
	rt_hw_cpu_reset();
	return RT_EOK;
}

/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(set_calc, set calc at 5A );
