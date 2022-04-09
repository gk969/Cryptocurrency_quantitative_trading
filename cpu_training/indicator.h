#pragma once

void indicator_test();

class Indicator_SMA {
private:
    D_FLOAT* sma;
    int len;
    int mean_size;
public:
    Indicator_SMA(Kline_item* kline, int len, int mean_size);
    ~Indicator_SMA();
    
    inline D_FLOAT get(int i);
};

class Indicator_EMA {
private:
    D_FLOAT* ema;
    int len;
    int mean_size;
public:
    Indicator_EMA(Kline_item* kline, int len, int mean_size);
    ~Indicator_EMA();
    
    inline D_FLOAT get(int i);
};

typedef struct {
    D_FLOAT median;
    D_FLOAT upper;
    D_FLOAT lower;
}Bollinger_band;

class Indicator_bollinger_band {
private:
    Bollinger_band* band;
    int len;
    int mean_size;
    int multiplier;
    
    void init(Kline_item* kline, int len, int mean_size, int multiplier);
public:
    Indicator_bollinger_band(Kline_item* kline, int len);
    Indicator_bollinger_band(Kline_item* kline, int len, int mean_size, int multiplier);
    ~Indicator_bollinger_band();
    
    inline Bollinger_band* get(int i);
};


