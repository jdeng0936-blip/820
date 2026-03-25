#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_ext_io.h>
#include <dfs.h>
#include <dfs_fs.h>
#include <lvgl.h>
#include <lcd_port.h>
#include "main.h"
#define LED_THREAD_STACK_SIZE 512
#define LED_THREAD_PRIO (RT_THREAD_PRIORITY_MAX - 2)
static rt_uint8_t * led_thread_stack;
static struct rt_thread led_thread;

extern struct rt_memheap system_heap;
extern volatile uint8_t operation_running;

extern volatile uint8_t line_lost;

extern volatile uint8_t mqtt_online;
volatile uint8_t last_error_is_perm = 0;

extern uint8_t ucSDiscInBuf[];

static void led_thread_entry(void *parameter)
{
	rt_uint32_t period, pulse, dir;
	struct rt_device_pwm * pwm_dev = (struct rt_device_pwm *)rt_device_find("pwm12");
	period = 100;
	dir = 1;
	pulse = 0;
	rt_pwm_set(pwm_dev, 2, period, pulse);
	rt_pwm_enable(pwm_dev, 2);
	rt_pin_write(120, PIN_LOW);
	rt_pin_mode(120, PIN_MODE_OUTPUT);
	rt_pin_write(123, PIN_LOW);
	rt_pin_mode(123, PIN_MODE_OUTPUT);
	rt_pin_write(122, PIN_LOW);
	rt_pin_mode(122, PIN_MODE_OUTPUT);
	rt_pin_write(124, PIN_LOW);
	rt_pin_mode(124, PIN_MODE_OUTPUT);
	rt_pin_write(118, PIN_LOW);
	rt_pin_mode(118, PIN_MODE_OUTPUT);
	while(1)
	{
		rt_thread_mdelay(50);
		if (dir)
		{
				pulse += 5;
		}
		else
		{
				pulse -= 5;
		}
		if (pulse >= period)
		{
				dir = 0;
		}
		if (25 == pulse)
		{
				dir = 1;
		}
		rt_pwm_set(pwm_dev, 2, period, pulse);
		
		if(operation_running == 1)
		{
			rt_pin_write(120, PIN_HIGH);
			ucSDiscInBuf[0] |= (1<<0);
		}
		else
		{
			rt_pin_write(120, PIN_LOW);
			ucSDiscInBuf[0] &=~ (1<<0);
		}
		
		if(line_error == 1)
		{
			rt_pin_write(123, PIN_HIGH);
		}
		else
		{
			rt_pin_write(123, PIN_LOW);
		}
		
		if(line_lost == 1)
		{
			rt_pin_write(124, PIN_HIGH);
			ucSDiscInBuf[0] |= (1<<2);
		}
		else
		{
			rt_pin_write(124, PIN_LOW);
			ucSDiscInBuf[0] &=~ (1<<2);
		}
		
		if((line_gnd == 1)||(last_error_is_perm == 1))
		{
			rt_pin_write(122, PIN_HIGH);
			ucSDiscInBuf[0] |= (1<<3);
		}
		else
		{
			rt_pin_write(122, PIN_LOW);
			ucSDiscInBuf[0] &=~ (1<<3);
		}
		
		if(mqtt_online == 1)
		{
			rt_pin_write(118, PIN_HIGH);
			ucSDiscInBuf[0] |= (1<<1);
		}
		else
		{
			rt_pin_write(118, PIN_LOW);
			ucSDiscInBuf[0] &=~ (1<<1);
		}
	}
}

static int led_thread_init(void)
{
	rt_err_t err;
	
	led_thread_stack = rt_memheap_alloc(&system_heap, LED_THREAD_STACK_SIZE);

	err = rt_thread_init(&led_thread, "LED", led_thread_entry, RT_NULL,
			 led_thread_stack, LED_THREAD_STACK_SIZE, LED_THREAD_PRIO, 10);
	if(err != RT_EOK)
	{
		rt_kprintf("Failed to create LED thread");
		return -1;
	}
	rt_thread_startup(&led_thread);

	return 0;
}
INIT_ENV_EXPORT(led_thread_init);
