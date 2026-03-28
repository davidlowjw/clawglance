#pragma once

#include <stdbool.h>

void wifi_mgr_init(const char *ssid, const char *pass);
bool wifi_mgr_wait(int timeout_ms);
bool wifi_mgr_is_connected(void);
void wifi_mgr_get_ip(char *buf, int buf_len);
