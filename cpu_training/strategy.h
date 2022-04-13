#pragma once

void test_ema_cross(Kline_item* kline, int len, D_FLOAT* value, int fast_ema, int slow_ema);
void test_macd(Kline_item* kline, int len, D_FLOAT* value, int fast_ema, int slow_ema, signal_ema);