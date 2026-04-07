/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <dfs.h>
#include <dfs_fs.h>
#include <dfs_file.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
#define	CALC_VER	"V4.0.5BNC"
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SPI4_NSS_Pin GPIO_PIN_4
#define SPI4_NSS_GPIO_Port GPIOE
#define OUT3_Pin GPIO_PIN_13
#define OUT3_GPIO_Port GPIOC
#define OUT2_Pin GPIO_PIN_10
#define OUT2_GPIO_Port GPIOI
#define OUT1_Pin GPIO_PIN_11
#define OUT1_GPIO_Port GPIOI
#define INPUT1_Pin GPIO_PIN_8
#define INPUT1_GPIO_Port GPIOF
#define INPUT2_Pin GPIO_PIN_9
#define INPUT2_GPIO_Port GPIOF
#define INPUT4_Pin GPIO_PIN_10
#define INPUT4_GPIO_Port GPIOF
#define TEST_LED_Pin GPIO_PIN_1
#define TEST_LED_GPIO_Port GPIOA
#define AD_INT_Pin GPIO_PIN_1
#define AD_INT_GPIO_Port GPIOB
#define AD_INT_EXTI_IRQn EXTI1_IRQn
#define FAB_EN_Pin GPIO_PIN_9
#define FAB_EN_GPIO_Port GPIOH
#define AD_RESET_Pin GPIO_PIN_7
#define AD_RESET_GPIO_Port GPIOG
#define AD_START_Pin GPIO_PIN_6
#define AD_START_GPIO_Port GPIOC
#define OUT4_Pin GPIO_PIN_3
#define OUT4_GPIO_Port GPIOD
#define AD_RANGE_Pin GPIO_PIN_10
#define AD_RANGE_GPIO_Port GPIOG
#define OUT5_Pin GPIO_PIN_11
#define OUT5_GPIO_Port GPIOG
#define INPUT7_Pin GPIO_PIN_14
#define INPUT7_GPIO_Port GPIOG
#define INPUT6_Pin GPIO_PIN_5
#define INPUT6_GPIO_Port GPIOB
#define I2C1_SCL_Pin GPIO_PIN_6
#define I2C1_SCL_GPIO_Port GPIOB
#define INPUT5_Pin GPIO_PIN_7
#define INPUT5_GPIO_Port GPIOB
#define INPUT3_Pin GPIO_PIN_8
#define INPUT3_GPIO_Port GPIOB
#define I2C1_SDA_Pin GPIO_PIN_9
#define I2C1_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
union LINE_STATUS{
	uint8_t status;
	struct{
		uint8_t la_st:2;
		uint8_t lb_st:2;
		uint8_t lc_st:2;
		uint8_t l0_st:2;
	}bits;
};

typedef struct{
	uint16_t sign;					//2
	uint16_t pt;					//2
	uint16_t ct;					//2
	float i0_limt;				//4
	uint16_t i0_ct;				//2
	uint8_t v_over;				//1
	uint8_t v_drop;				//1
	uint8_t v_zero;				//1
	uint8_t u0_limt;			//1
	float rms_ratio_ua;		//4
	float rms_ratio_ub;		//4
	float rms_ratio_uc;		//4
	float rms_ratio_u0;		//4
	float rms_ratio_ia;		//4
	float rms_ratio_ib;		//4
	float rms_ratio_ic;		//4
	float rms_ratio_i0;		//4
	char ver[16];					//16	
}SETTINGS;

typedef struct {
	uint8_t work_status;					//all led related status
	uint8_t out_status;						//out channel status
	uint16_t clients_cnt;					//how many online sensor
	uint32_t clients_mask[32];		//bitmask for online clients
	uint8_t in_status;						//in channel status
	uint8_t error_type;
	uint8_t error_line;
	uint8_t error_slow;
	uint16_t error_id_1;
	uint16_t error_id_2;
	uint16_t error_id_3;
	uint32_t error_ts;
	float error_ua;
	float error_ub;
	float error_uc;
	float error_u0;
	float rms_1;
	float rms_2;
	float rms_3;
	SETTINGS settings;
}CALC_STATUS;

extern struct rt_memheap system_heap;

extern volatile uint8_t fast_error;
extern uint8_t fast_error_line;
extern time_t fast_error_ts;
extern volatile uint8_t i2c_check_ok;

extern volatile uint8_t slow_error;
extern volatile uint8_t slow_error_ack;
extern uint8_t slow_gnd_line;
extern uint8_t slow_error_type;
extern time_t slow_error_ts;
extern volatile uint8_t settings_changed;
extern void save_settings(void);

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
