#pragma once
#include <stdbool.h>
#include <stdint.h>

void CloseClient();

void WaitForConnection();

bool Send(const void* data, uint32_t len, uint32_t ts);