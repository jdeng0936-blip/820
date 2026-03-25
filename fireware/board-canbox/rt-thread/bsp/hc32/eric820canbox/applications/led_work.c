#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#include <drv_can.h>

/* defined the LED_GREEN pin: PD4 */
#define COMM_LED_PIN GET_PIN(B, 3)
#define PWR_LED_PIN GET_PIN(B, 4)

#define LED_WORK_THREAD_STACK_SIZE 512
#define LED_WORK_THREAD_PRIORITY		29
#define LED_WORK_THREAD_SLICE_MS		5

extern volatile uint8_t can_online;

extern uint8_t can_idle;
extern void _set_default_filter(void);
extern rt_device_t can_dev;
typedef struct
{
    struct rt_can_device rt_can;
    struct can_dev_init_params init;
    CM_CAN_TypeDef *instance;
    stc_can_init_t ll_init;
} can_device;
void led_work(void * param)
{
	while(1)
	{
		//rt_pin_write(PWR_LED_PIN, PIN_LOW);
		rt_thread_mdelay(1000);
		//rt_pin_write(PWR_LED_PIN, PIN_HIGH);
		//rt_thread_mdelay(500);
		if(can_online == 1)
		{
			rt_pin_write(COMM_LED_PIN, PIN_LOW);
			rt_pin_write(PWR_LED_PIN, PIN_HIGH);
		}
		else
		{
			rt_pin_write(PWR_LED_PIN, PIN_LOW);
			rt_pin_write(COMM_LED_PIN, PIN_HIGH);
		}
		can_idle++;
		if(can_idle == 6)
		{
			can_idle = 0;
			can_online = 0;
			can_device *p_can_dev = (can_device *)rt_container_of(can_dev, can_device, rt_can);
			CAN_DeInit(p_can_dev->instance);
			rt_thread_mdelay(10);
			_set_default_filter();
			//rt_hw_cpu_reset();
		}
	}
}
int led_work_init(void)
{
	rt_thread_t led_work_thread;
	
	GPIO_SetDebugPort(GPIO_PIN_SWO, DISABLE);
	GPIO_SetDebugPort(GPIO_PIN_TRST, DISABLE);
	rt_pin_mode(COMM_LED_PIN, PIN_MODE_OUTPUT);
	rt_pin_mode(PWR_LED_PIN, PIN_MODE_OUTPUT);
	rt_pin_write(COMM_LED_PIN, PIN_HIGH);
	rt_pin_write(PWR_LED_PIN, PIN_HIGH);	

	led_work_thread = rt_thread_create("led_th", led_work, RT_NULL,
														LED_WORK_THREAD_STACK_SIZE,
														LED_WORK_THREAD_PRIORITY,
														LED_WORK_THREAD_SLICE_MS);
	rt_thread_startup(led_work_thread);
	
	return 0;
}
INIT_DEVICE_EXPORT(led_work_init);
