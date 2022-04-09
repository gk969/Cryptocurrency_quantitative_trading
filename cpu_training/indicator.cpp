#include "common.h"
#include "kline_data.h"

void indicator_test() {
#define TEST_LEN    100
    Kline_item* kline = read_kline("market.ethusdt.kline.1day", 0, TEST_LEN);
    log_ln("close:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", kline[i].close);
    }
    log_ln("");
    
    Indicator_SMA* sma = new Indicator_SMA(&kline[0], TEST_LEN, 3);
    log_ln("sma:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", sma->get(i));
    }
    log_ln("");
    delete sma;
    
    Indicator_EMA* ema = new Indicator_EMA(&kline[0], TEST_LEN, 3);
    log_ln("ema3:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", ema->get(i));
    }
    log_ln("");
    delete ema;
    
    ema = new Indicator_EMA(&kline[0], TEST_LEN, 5);
    log_ln("ema5:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", ema->get(i));
    }
    log_ln("");
    delete ema;
    
    
    Indicator_bollinger_band* bolling_band = new Indicator_bollinger_band(&kline[0], TEST_LEN);
    log_ln("bolling_band:");
    for (int i = 0; i < TEST_LEN; i++) {
        Bollinger_band* bb=bolling_band->get(i);
        log_ln("%.2f %.2f %.2f", bb->median, bb->upper, bb->lower);
    }
    log_ln("");
    delete bolling_band;
    
    
}



Indicator_SMA::Indicator_SMA(Kline_item* kline, int len, int mean_size) {
    this->len = len;
    this->mean_size = mean_size;
    
    if (len < mean_size) {
        log_ln("Indicator_SMA Error len < mean_size");
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    sma = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    if (sma == NULL) {
        log_ln("Indicator_SMA Error malloc");
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    for (; i < (mean_size - 1); i++) {
        sma[i] = -1;
        sum += kline[i].close;
    }
    
    int last_step = mean_size - 1;
    for (; i < len; i++) {
        sum += kline[i].close;
        sma[i] = sum / mean_size;
        sum -= kline[i - last_step].close;
    }
}

Indicator_SMA::~Indicator_SMA() {
    free(sma);
}

D_FLOAT Indicator_SMA::get(int i) {
    return sma[i];
}


// EMAtoday = α * Pricetoday + ( 1 - α ) * EMAyesterday;
// α = 2 / (1 + d)

// ema3
// 309.58 397.07 716.74 1105.5 868.28
//               474.46 789.98 829.13
// ema5
// 309.58 397.07 716.74 1105.5 868.28 402.52 683.26
//                             679.43 587.12 619.17
Indicator_EMA::Indicator_EMA(Kline_item* kline, int len, int mean_size) {
    this->len = len;
    this->mean_size = mean_size;
    
    if (len < mean_size) {
        log_ln("Indicator_EMA Error len < mean_size");
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    ema = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    if (ema == NULL) {
        log_ln("Indicator_EMA Error malloc");
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    for (; i < (mean_size - 1); i++) {
        ema[i] = -1;
        sum += kline[i].close;
    }
    sum += kline[i].close;
    ema[i] = sum / mean_size;
    i++;
    
    D_FLOAT factor = 2.0f / (1 + mean_size);
    D_FLOAT last_factor = 1 - factor;
    for (; i < len; i++) {
        ema[i] = factor * kline[i].close + last_factor * ema[i - 1];
    }
}

Indicator_EMA::~Indicator_EMA() {
    free(ema);
}

D_FLOAT Indicator_EMA::get(int i) {
    return ema[i];
}



Indicator_bollinger_band::Indicator_bollinger_band(Kline_item* kline, int len) {
    init(kline, len, 20, 2);
}

Indicator_bollinger_band::Indicator_bollinger_band(Kline_item* kline, int len, int mean_size, int multiplier) {
    init(kline, len, mean_size, multiplier);
}

void Indicator_bollinger_band::init(Kline_item* kline, int len, int mean_size, int multiplier) {
    this->len = len;
    this->mean_size = mean_size;
    this->multiplier = multiplier;
    
    if (len < mean_size) {
        log_ln("Indicator_bollinger_band Error len < mean_size");
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    band = (Bollinger_band*)malloc(len * sizeof(Bollinger_band));
    if (band == NULL) {
        log_ln("Indicator_bollinger_band Error malloc");
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    for (; i < (mean_size - 1); i++) {
        band[i].median = 0;
        band[i].upper = 0;
        band[i].lower = 0;
        sum += kline[i].close;
    }
    
    int last_step = mean_size - 1;
    for (; i < len; i++) {
        sum += kline[i].close;
        D_FLOAT mean = sum / mean_size;
        sum -= kline[i - last_step].close;
        
        band[i].median = mean;
        
        D_FLOAT square_deviation = 0;
        for (int d = 0; d < mean_size; d++) {
            D_FLOAT diff = kline[i - d].close - mean;
            square_deviation += diff * diff;
        }
        D_FLOAT standard_deviation = sqrt(square_deviation / mean_size);
        standard_deviation *= multiplier;
        band[i].upper = mean + standard_deviation;
        band[i].lower = mean - standard_deviation;
    }
}

Indicator_bollinger_band::~Indicator_bollinger_band() {
    free(band);
}

Bollinger_band* Indicator_bollinger_band::get(int i) {
    return &band[i];
}

