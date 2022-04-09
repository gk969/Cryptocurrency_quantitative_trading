#pragma once

#include "common.h"


void kline_csv_to_bin(int kline_period, const char* csv_file_name, const char* bin_file_name);
void all_kline_csv_to_bin();
Kline_item* read_kline(const char* kline_ch, int start, int len);

