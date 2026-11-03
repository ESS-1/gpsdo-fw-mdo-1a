/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define GPS_LOCK_OUTPUT_Pin GPIO_PIN_0
#define GPS_LOCK_OUTPUT_GPIO_Port GPIOA
#define PPB_LOCK_OUTPUT_Pin GPIO_PIN_1
#define PPB_LOCK_OUTPUT_GPIO_Port GPIOA
#define LCD_CS_Pin GPIO_PIN_2
#define LCD_CS_GPIO_Port GPIOA
#define LCD_DC_Pin GPIO_PIN_3
#define LCD_DC_GPIO_Port GPIOA
#define LCD_RES_Pin GPIO_PIN_4
#define LCD_RES_GPIO_Port GPIOA
#define LCD_SCL_Pin GPIO_PIN_5
#define LCD_SCL_GPIO_Port GPIOA
#define OCXO_EN_Pin GPIO_PIN_6
#define OCXO_EN_GPIO_Port GPIOA
#define LCD_SDA_Pin GPIO_PIN_7
#define LCD_SDA_GPIO_Port GPIOA
#define VUSB_SEN_Pin GPIO_PIN_0
#define VUSB_SEN_GPIO_Port GPIOB
#define PPS_OUTPUT_Pin GPIO_PIN_1
#define PPS_OUTPUT_GPIO_Port GPIOB
#define LED1_Pin GPIO_PIN_2
#define LED1_GPIO_Port GPIOB
#define GPS_TX_Pin GPIO_PIN_10
#define GPS_TX_GPIO_Port GPIOB
#define GPS_RX_Pin GPIO_PIN_11
#define GPS_RX_GPIO_Port GPIOB
#define USB_DP_PULLUP_Pin GPIO_PIN_14
#define USB_DP_PULLUP_GPIO_Port GPIOB
#define PPS_Pin GPIO_PIN_8
#define PPS_GPIO_Port GPIOA
#define VCO_CONTROL_Pin GPIO_PIN_9
#define VCO_CONTROL_GPIO_Port GPIOA
#define LCD_BL_PWM_Pin GPIO_PIN_10
#define LCD_BL_PWM_GPIO_Port GPIOA
#define ROTARY_PRESS_Pin GPIO_PIN_5
#define ROTARY_PRESS_GPIO_Port GPIOB
#define ROTARY_PRESS_EXTI_IRQn EXTI9_5_IRQn
#define ROTARY_A_Pin GPIO_PIN_6
#define ROTARY_A_GPIO_Port GPIOB
#define ROTARY_B_Pin GPIO_PIN_7
#define ROTARY_B_GPIO_Port GPIOB
#define PLL_SCL_Pin GPIO_PIN_8
#define PLL_SCL_GPIO_Port GPIOB
#define PLL_SDA_Pin GPIO_PIN_9
#define PLL_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
