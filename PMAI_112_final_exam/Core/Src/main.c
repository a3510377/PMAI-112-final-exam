/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#include "IIC_1602.h"
#include "stdio.h"

#define NO (11)
#define DATE ("05/27")

#define PB4 HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)
#define PB5(state) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, state)

const uint16_t TONE_TABLE[] = {0,    1912, 1704, 1518, 1432,
                               1276, 1136, 1012, 956};

const uint8_t MODE1_DUTY_LEVEL[] = {0, 25, 50, 100};
const uint8_t FREQ_TIME_MAP[] = {10, 5, 2};

uint8_t tmp1 = 0, tmp2 = 0, tmp3 = 0, old_mode = 0, ref[5] = {0};
uint16_t time_count1 = 0;

void tone(uint8_t index) {
  TIM2->ARR = TONE_TABLE[index];
  TIM2->CCR1 = TONE_TABLE[index] / 2;
}
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_TIM6_Init();
  MX_USART2_UART_Init();
  MX_ADC_Init();
  MX_I2C1_Init();
  MX_TIM21_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim6);
  Initialize_LCD(hi2c1);

  clear_LCD();

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  for (uint8_t i = 1; i < 9; i++) {
    tone(i);
    GPIOC->ODR = 1 << (i - 1);
    HAL_Delay(400);
  }
  GPIOC->ODR = 0;
  HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);

  char buf[20];
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    uint8_t mode = ~GPIOB->IDR & 0x03;
    sprintf(buf, "MODE%d", mode);
    writes(0x80, buf);

    // reset variables
    if (old_mode != mode) {
      clear_LCD();
      HAL_ADC_Stop(&hadc);
      HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
      TIM2->ARR = 99;
      TIM2->CCR2 = TIM2->CCR3 = 0;
      tmp1 = tmp2 = tmp3 = time_count1 = 0;
      old_mode = mode;
    }

    switch (mode) {
      case 0: {
        // MODE0 / NO : 11
        sprintf(buf, " / NO : %02d", NO);
        writes(0x85, buf);
        // 01/01_cnt : 000
        sprintf(buf, "%s_cnt : %03d", DATE, tmp1);
        writes(0xc0, buf);

        if (!time_count1) {
          time_count1 = 100;  // every 100ms + 1
          tmp1 = (tmp1 + 1) % 101;
        }
      } break;
      case 1: {
        // MODE1 :adc1/adc2
        sprintf(buf, " :adc1/adc2");
        writes(0x85, buf);

        HAL_ADC_Start(&hadc);
        HAL_ADC_PollForConversion(&hadc, 1);
        tmp1 = HAL_ADC_GetValue(&hadc) * (100.0 / 1024);

        HAL_ADC_Start(&hadc);
        HAL_ADC_PollForConversion(&hadc, 1);
        tmp2 = HAL_ADC_GetValue(&hadc) * (100.0 / 1024);

        // D1=00o/oD2=00o/o
        sprintf(buf, "D1=%02do/oD2=%02do/o", tmp1, tmp2);
        writes(0xc0, buf);

        TIM2->CCR2 = tmp1;
        TIM2->CCR3 = MODE1_DUTY_LEVEL[(tmp2 - 1) / 25];
      } break;
      case 2: {
        // MODE2 : HC_SR04
        sprintf(buf, " : HC_SR04");
        writes(0x85, buf);

        if (!time_count1) {
          time_count1 = 50;

          for (char i = 0; i < 15; i++) PB5(GPIO_PIN_SET);
          PB5(GPIO_PIN_RESET);

          while (!PB4);
          __HAL_TIM_SET_COUNTER(&htim21, 0);
          HAL_TIM_Base_Start(&htim21);
          while (PB4);
          HAL_TIM_Base_Stop(&htim21);
          uint8_t distance = __HAL_TIM_GetCounter(&htim21) / 58;

          if (distance > 99) distance = 99;

          int tmp = 0;
          for (uint8_t j = 0; j < 5; j++) {
            tmp += ref[j];
            if (j > 0) ref[j - 1] = ref[j];
          }
          ref[4] = distance;
          distance = tmp / 5;

          tmp = distance / 10;
          uint8_t step = tmp > 3 ? 0 : 3 - tmp;
          sprintf(buf, "step: %d dist:%03d", step, distance);
          writes(0xc0, buf);

          if (step != tmp1 || !step) {
            tmp1 = step;
            tmp3 = tmp2 = 0;
            HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
          }

          if (step != 0) {
            if (tmp3 > 0) tmp3--;
            else {
              tmp3 = FREQ_TIME_MAP[step - 1];
              tmp2 = !tmp2;
              tone(tmp2);
              HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
            }
          }
        }
      } break;
      case 3: {
        // MODE3 : 2_PWM
        sprintf(buf, " : 2_PWM");
        writes(0x85, buf);
        // PWM1:00 PWM2:00
        sprintf(buf, "PWM1:%02d PWM2:%02d", tmp1, tmp2);
        writes(0xc0, buf);

        if (!time_count1) {
          time_count1 = 10;  // every 10ms + 1

          // (0 || 2) +1
          // (1 || 3) -1
          int x = tmp3 % 2 ? -1 : 1;

          // 2, 3
          if (tmp3 / 2) tmp2 += x;
          // 0, 1
          else tmp1 += x;

          if ((!tmp1 && !tmp2) || tmp1 == 99 || tmp2 == 99) {
            tmp3 = (tmp3 + 1) % 4;
          }

          TIM2->CCR2 = tmp1;
          TIM2->CCR3 = tmp2;
        }
      } break;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) {
    if (time_count1 > 0) time_count1--;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
