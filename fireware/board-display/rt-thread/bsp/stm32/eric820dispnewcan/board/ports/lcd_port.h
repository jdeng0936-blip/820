/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-01-08     zylx         first version
 */

#ifndef __LCD_PORT_H__
#define __LCD_PORT_H__

#include <rtthread.h>
#include <drivers/classes/graphic.h>

/* armfly 5 inch screen, 800 * 480 */
#define LCD_WIDTH           800
#define LCD_HEIGHT          480
#define LCD_BITS_PER_PIXEL  16
#define LCD_BUF_SIZE        (LCD_WIDTH * LCD_HEIGHT * LCD_BITS_PER_PIXEL / 8)
#define LCD_PIXEL_FORMAT    RTGRAPHIC_PIXEL_FORMAT_RGB565

#define LCD_HSYNC_WIDTH     10
#define LCD_VSYNC_HEIGHT    1
#define LCD_HBP             46
#define LCD_VBP             25
#define LCD_HFP             10
#define LCD_VFP             10

#define LCD_BACKLIGHT_USING_PWM
#define LCD_PWM_DEV_NAME    "pwm1"
#define LCD_PWM_DEV_CHANNEL 4

extern volatile uint8_t backlight_status;

/* armfly 5 inch screen, 800 * 480 */

struct drv_lcd_device
{
    struct rt_device parent;

    struct rt_device_graphic_info lcd_info;

    struct rt_semaphore lcd_lock;

    /* 0:front_buf is being used 1: back_buf is being used*/
    rt_uint8_t cur_buf;
    rt_uint8_t *front_buf;
    rt_uint8_t *back_buf;
};

#endif /* __LCD_PORT_H__ */
