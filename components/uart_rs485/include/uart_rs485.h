#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define TX_BUF_SIZE (512)
#define RX_BUF_SIZE (1024)

void uart_init(uart_port_t uart_num);
