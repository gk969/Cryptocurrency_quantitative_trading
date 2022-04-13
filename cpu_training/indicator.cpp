#include "common.h"

void indicator_test() {
#define TEST_LEN    1610
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
    
    Indicator_EMA* ema = new Indicator_EMA(&kline[0], TEST_LEN, 12);
    log_ln("ema12:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", ema->get(i));
    }
    log_ln("");
    delete ema;
    
    ema = new Indicator_EMA(&kline[0], TEST_LEN, 26);
    log_ln("ema26:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", ema->get(i));
    }
    log_ln("");
    delete ema;
    
    
    // Indicator_bollinger_band* bolling_band = new Indicator_bollinger_band(&kline[0], TEST_LEN);
    // log_ln("bolling_band:");
    // for (int i = 0; i < TEST_LEN; i++) {
    // Bollinger_band* bb=bolling_band->get(i);
    // log_ln("%.2f %.2f %.2f", bb->median, bb->upper, bb->lower);
    // }
    // log_ln("");
    // delete bolling_band;
    
    
    Indicator_MACD* macd = new Indicator_MACD(&kline[0], TEST_LEN);
    log_ln("macd:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", macd->get(i));
    }
    log_ln("");
    delete macd;
    
    
    Indicator_RSI* rsi = new Indicator_RSI(&kline[0], TEST_LEN);
    log_ln("rsi:");
    for (int i = 0; i < TEST_LEN; i++) {
        log("%.2f ", rsi->get(i));
    }
    log_ln("");
    delete rsi;
    
}



Indicator_SMA::Indicator_SMA(Kline_item* kline, int len, int mean_size) {
    this->len = len;
    this->mean_size = mean_size;
    
    if (len < mean_size) {
        log_ln("Indicator_SMA Error len < mean_size");
        exit(0);
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    sma = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    if (sma == NULL) {
        log_ln("Indicator_SMA Error malloc");
        exit(0);
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    int start = get_valid_start();
    for (; i < start; i++) {
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

int Indicator_SMA::get_valid_start() {
    return mean_size - 1;
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
        exit(0);
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    ema = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    if (ema == NULL) {
        log_ln("Indicator_EMA Error malloc");
        exit(0);
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    int start = get_valid_start();
    for (; i < start; i++) {
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

int Indicator_EMA::get_valid_start() {
    return mean_size - 1;
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
        exit(0);
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    band = (Bollinger_band*)malloc(len * sizeof(Bollinger_band));
    if (band == NULL) {
        log_ln("Indicator_bollinger_band Error malloc");
        exit(0);
        return;
    }
    
    D_FLOAT sum = 0;
    int i = 0;
    int start = get_valid_start();
    for (; i < start; i++) {
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

int Indicator_bollinger_band::get_valid_start() {
    return mean_size - 1;
}


void Indicator_MACD::init(Kline_item* kline, int len, int ema_fast, int ema_slow, int signal) {
    this->ema_fast = ema_fast;
    this->ema_slow = ema_slow;
    this->signal = signal;
    
    Indicator_EMA* fast_ema = new Indicator_EMA(kline, len, ema_fast);
    Indicator_EMA* slow_ema = new Indicator_EMA(kline, len, ema_slow);
    Kline_item* diff = (Kline_item*)malloc(len * sizeof(Kline_item));
    if (diff == NULL) {
        log_ln("Indicator_MACD Error diff malloc");
        exit(0);
        return;
    }
    
    int diff_start = ema_slow - 1;
    for (int i = 0; i < diff_start; i++) {
        diff[i].close = 0;
    }
    
    // log_ln("macd diff");
    for (int i = diff_start; i < len; i++) {
        diff[i].close = fast_ema->get(i) - slow_ema->get(i);
        // log("%.2f ", diff[i].close);
    }
    // log_ln("");
    delete fast_ema;
    delete slow_ema;
    
    
    Indicator_EMA* signal_ema = new Indicator_EMA(&diff[diff_start], len - diff_start, signal);
    
    macd = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    
    int macd_start = diff_start + signal - 1;
    for (int i = 0; i < macd_start; i++) {
        macd[i] = 0;
    }
    
    // log_ln("macd signal_ema");
    for (int i = macd_start; i < len; i++) {
        // log("%.2f - %.2f   ", diff[i].close, signal_ema->get(i - ema_slow + 1));
        macd[i] = diff[i].close - signal_ema->get(i - ema_slow + 1);
    }
    // log_ln("");
    
    free(diff);
    delete signal_ema;
}

Indicator_MACD::Indicator_MACD(Kline_item* kline, int len, int ema_fast, int ema_slow, int signal) {
    init(kline, len, ema_fast, ema_slow, signal);
}

Indicator_MACD::Indicator_MACD(Kline_item* kline, int len) {
    init(kline, len, 12, 26, 9);
}

Indicator_MACD::~Indicator_MACD() {
    free(macd);
}

D_FLOAT Indicator_MACD::get(int i) {
    return macd[i];
}

int Indicator_MACD::get_valid_start() {
    return ema_slow - 1 + signal - 1;
}


void Indicator_RSI::init(Kline_item* kline, int len, int mean_size) {
    this->len = len;
    this->mean_size = mean_size;
    
    if (len < mean_size) {
        log_ln("Indicator_RSI Error len < mean_size");
        exit(0);
        return;
    }
    
    mean_size = imax(mean_size, 1);
    
    rsi = (D_FLOAT*)malloc(len * sizeof(D_FLOAT));
    if (rsi == NULL) {
        log_ln("Indicator_RSI Error malloc");
        exit(0);
        return;
    }
    
    D_FLOAT gain_sum = 0;
    D_FLOAT loss_sum = 0;
    rsi[0] = -1;
    int i = 1;
    for (; i < mean_size + 1; i++) {
        rsi[i] = -1;
        D_FLOAT diff = kline[i].close - kline[i - 1].close;
        if (diff >= 0) {
            gain_sum += diff;
        } else {
            loss_sum += 0 - diff;
        }
    }
    D_FLOAT avg_gain = gain_sum / mean_size;
    D_FLOAT avg_loss = loss_sum / mean_size;
    rsi[i - 1] = 100 - (100 / (1 + avg_gain / avg_loss));
    
    int last_step = mean_size - 1;
    for (; i < len; i++) {
        D_FLOAT diff = kline[i].close - kline[i - 1].close;
        D_FLOAT cur_gain = 0;
        D_FLOAT cur_loss = 0;
        if (diff >= 0) {
            cur_gain = diff;
        } else {
            cur_loss = 0 - diff;
        }
        
        avg_gain = (avg_gain * last_step + cur_gain) / mean_size;
        avg_loss = (avg_loss * last_step + cur_loss) / mean_size;
        rsi[i] = 100 - (100 / (1 + avg_gain / avg_loss));
        
        // log_ln("cur_gain %.2f cur_loss %.2f avg_gain %.2f avg_loss %.2f rsi %.2f", cur_gain, cur_loss, avg_gain, avg_loss, rsi[i]);
    }
}

Indicator_RSI::Indicator_RSI(Kline_item* kline, int len, int mean_size) {
    init(kline, len, mean_size);
}

Indicator_RSI::Indicator_RSI(Kline_item* kline, int len) {
    init(kline, len, 14);
}

Indicator_RSI::~Indicator_RSI() {
    free(rsi);
}

D_FLOAT Indicator_RSI::get(int i) {
    return rsi[i];
}

int Indicator_RSI::get_valid_start() {
    return mean_size;
}

