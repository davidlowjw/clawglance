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

void rd_03_init(void);
void rd_03_read_task(void *arg);
extern int rd_03_read_num;
