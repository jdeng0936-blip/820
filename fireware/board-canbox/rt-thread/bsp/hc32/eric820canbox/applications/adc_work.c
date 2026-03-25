#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#define ADC_WORK_THREAD_STACK_SIZE 512
#define ADC_WORK_THREAD_PRIORITY		29
#define ADC_WORK_THREAD_SLICE_MS		5

extern volatile uint8_t can_upload_wave_req;
extern uint8_t can_tx_buffer[384];
uint16_t can_upload_wave_wait = 0;

rt_mq_t adc_data_mq;
rt_mq_t hist_data_mq;

rt_device_t hw_dev = RT_NULL;
rt_adc_device_t adc_dev = RT_NULL;

rt_err_t adc_timer_cb(rt_device_t dev, rt_size_t size)
{
	rt_uint16_t value = rt_adc_read(adc_dev, 7);
	int8_t val = (value>>4) - 128;
	rt_mq_send_interruptible(adc_data_mq, &val, 1);
	return RT_EOK;
}

void adc_work(void * param)
{
	int8_t value,dummy;
	rt_hwtimer_mode_t mode = HWTIMER_MODE_PERIOD;
	rt_hwtimerval_t timeout_s;
	
	adc_dev = (rt_adc_device_t)rt_device_find("adc1");
	
	rt_adc_enable(adc_dev, 7);
	
	hw_dev = rt_device_find("tmra_1");
	rt_device_open(hw_dev, RT_DEVICE_OFLAG_RDWR);
	rt_device_control(hw_dev, HWTIMER_CTRL_MODE_SET, &mode);
	
	rt_device_set_rx_indicate(hw_dev, adc_timer_cb);
	
	timeout_s.sec = 0;
	timeout_s.usec = 625;
	rt_device_write(hw_dev, 0, &timeout_s, sizeof(timeout_s));
	while(1)
	{
		rt_mq_recv(adc_data_mq, &value, 1, RT_WAITING_FOREVER);
		if(RT_EOK != rt_mq_send_killable(hist_data_mq, &value, 1))
		{
			rt_mq_recv(hist_data_mq, &dummy, 1, RT_WAITING_FOREVER);
			rt_mq_send(hist_data_mq, &value, 1);
			if(can_upload_wave_req == 1)
			{
				can_upload_wave_wait++;
				if(can_upload_wave_wait == 200)
				{
					can_upload_wave_wait = 0;
					//rt_kprintf("wave data:\r");
					for(int i=0;i<384;i++)
					{
						rt_mq_recv(hist_data_mq, &can_tx_buffer[i], 1, RT_WAITING_FOREVER);
						//rt_kprintf("%d,\r", (int8_t)can_tx_buffer[i]);
					}
					can_upload_wave_req = 0;
				}
			}
		}
	}
}
int adc_work_init(void)
{
	rt_thread_t adc_work_thread;
	
	adc_data_mq = rt_mq_create("adc_mq", 1, 1024, RT_IPC_FLAG_FIFO);
	hist_data_mq = rt_mq_create("hist_mq", 1, 384, RT_IPC_FLAG_FIFO);
	
	adc_work_thread = rt_thread_create("adc_th", adc_work, RT_NULL,
														ADC_WORK_THREAD_STACK_SIZE,
														ADC_WORK_THREAD_PRIORITY,
														ADC_WORK_THREAD_SLICE_MS);
	rt_thread_startup(adc_work_thread);
	
	return 0;
}
INIT_APP_EXPORT(adc_work_init);
