/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2018 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#define	DISP_VER	"V4.0.5BNC NO LOGO"
extern uint32_t udid[];
extern volatile uint8_t i2c_check_ok;
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
//void Error_Handler(void);

/* USER CODE BEGIN EFP */
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
}CALC_SETTINGS;

typedef struct{
	uint16_t pt;
	uint16_t ct;
	float i0_limt;
	uint8_t i0_ct;
	uint8_t v_over;
	uint8_t v_drop;
	uint8_t v_zero;
	uint8_t u0_limt;
}PART_SETTING;

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
	CALC_SETTINGS settings;
}CALC_STATUS;


typedef struct {
	uint32_t error_ts;
	uint8_t  error_line;
	uint8_t  error_id_1;
	uint8_t  error_id_2;
	uint8_t  error_id_3;
	uint8_t  error_type;
	uint8_t  error_slow;
	float error_ua;
	float error_ub;
	float error_uc;
	float error_u0;
	float rms_1;
	float rms_2;
	float rms_3;
	uint8_t sign;
	uint8_t witch;
}ERROR_INFO;

//Ua Ub Uc U0 
typedef struct {
	float ua;
	float ub;
	float uc;
	float u0;
	float ia;
	float ib;
	float ic;
	float i0;
}REALTIME_INFO;
extern REALTIME_INFO realtime_info;
//Ua Ub Uc U0 ID1 3I0 ID2 3I0 ID3 3I0 S/P EL TS
typedef struct {
	float ua;
	float ub;
	float uc;
	float u0;
	uint16_t id1;
	float rms1;
	uint16_t id2;
	float rms2;
	uint16_t id3;
	float rms3;
	uint8_t type;
	uint8_t line;
	uint32_t ts;
}GND_ERROR_INFO;
extern GND_ERROR_INFO gnd_error_info;
typedef struct {
	float ua;
	float ub;
	float uc;
	float u0;
	uint32_t ts;
}VOL_ERROR_INFO;
extern VOL_ERROR_INFO over_error_info;
extern VOL_ERROR_INFO drop_error_info;
extern VOL_ERROR_INFO pt_error_info;
extern VOL_ERROR_INFO short_error_info;
extern uint16_t usSRegHoldBuf[];
extern volatile uint8_t line_gnd;
extern volatile uint8_t last_error_is_perm;
extern volatile uint8_t perm_error_clr_req;
typedef struct{
uint16_t id;
uint8_t status;
char pos[29];
}NODE;

typedef struct {
	uint16_t sign;			//	128
	char server_addr[16];		//服务器地址		16
	uint16_t server_port;		//服务器端口号	18
	uint16_t rs485_baud;		//485波特率			20
	uint16_t rs485_id;			//485通讯地址		22
	
	uint16_t pt;				//PT变比						24
	uint16_t ct;				//CT变比						26
	float i0_limt;			//故障零序电流上限	30
	uint16_t i0_ct;			//零序CT变比				32
	
	uint8_t v_over;			//过压门限					33
	uint8_t v_drop;			//欠压门限					34
	uint8_t u0_limt;		//3U0电压门限				35
	
	char pass[8];		//密码									43
	uint16_t idle;			//屏幕空闲时间			45
	uint8_t bl_percent;	//背光亮度	46			46
	char mac[16];				//网卡MAC地址	62
	char self_addr[16];	//本机IP			78
	char netmask[16];		//子网掩码		94
	char gw[16];				//网关地址		110
	uint8_t use4g;			//是否开启4G	111
}SETTINGS;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */
extern void set_lcd_backlight_percent(uint8_t percent);
extern int w5500Start(int argc, char *argv[]);
extern int ppp_sample_start(void);
extern int mb_slave_sample(void);
extern void save_client_cnt(uint16_t cnt);
extern uint16_t load_client_cnt(void);
extern uint8_t ucSCoilBuf[];
extern volatile uint8_t line_error;
#define VOLTAGE_OVER		0xE1
#define VOLTAGE_DROP		0xE2
#define PT_ERROR				0xE3
#define SHORT_ERROR			0xE4

#define	PERI_GND								0x01
#define	PERM_GND								0x02

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
