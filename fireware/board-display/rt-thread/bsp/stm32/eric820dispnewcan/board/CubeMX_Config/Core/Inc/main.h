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

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define COMM2_Pin GPIO_PIN_10
#define COMM2_GPIO_Port GPIOI
#define COMM1_Pin GPIO_PIN_11
#define COMM1_GPIO_Port GPIOI
#define G4_PWR_Pin GPIO_PIN_8
#define G4_PWR_GPIO_Port GPIOF
#define BEEP_Pin GPIO_PIN_3
#define BEEP_GPIO_Port GPIOC
#define HUM_DET_Pin GPIO_PIN_4
#define HUM_DET_GPIO_Port GPIOH
#define SPI3_NSS_Pin GPIO_PIN_4
#define SPI3_NSS_GPIO_Port GPIOA
#define TC_INT_Pin GPIO_PIN_7
#define TC_INT_GPIO_Port GPIOH
#define TC_INT_EXTI_IRQn EXTI9_5_IRQn
#define SPI2_NSS_Pin GPIO_PIN_12
#define SPI2_NSS_GPIO_Port GPIOB
#define W5500_RST_Pin GPIO_PIN_13
#define W5500_RST_GPIO_Port GPIOD
#define W5500_INT_Pin GPIO_PIN_3
#define W5500_INT_GPIO_Port GPIOG
#define AD_RESET_Pin GPIO_PIN_7
#define AD_RESET_GPIO_Port GPIOG
#define AD_START_Pin GPIO_PIN_6
#define AD_START_GPIO_Port GPIOC
#define TC_SCL_Pin GPIO_PIN_6
#define TC_SCL_GPIO_Port GPIOB
#define TC_SDA_Pin GPIO_PIN_9
#define TC_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
