#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctime>

#include <windows.h>
#include <cuda_runtime.h>
#include "cuda_inc/helper_cuda.h"
#include "../cpp_common/utils.h"

void test_vector_add();
int deviceQuery();

#define D_FLOAT float


#pragma pack(1)
typedef struct {
    D_FLOAT open;
    D_FLOAT close;
    D_FLOAT high;
    D_FLOAT low;
    D_FLOAT amount;
    D_FLOAT vol;
    int count;
} Kline_item;

typedef struct {
    double open;
    double close;
    double high;
    double low;
    double amount;
    double vol;
    int count;
} Kline_item_sto;
#pragma pack()

void kline_csv_to_bin(const char* csv_file_name, const char* bin_file_name) {
    log_ln("%s\n%s\n", csv_file_name, csv_file_name);
    
    FILE* csv_file = fopen(csv_file_name, "r");
    if (csv_file == NULL) {
        log_ln("open %s fail", csv_file_name);
        return;
    }
    
    FILE* bin_file = fopen(bin_file_name, "wb");
    if (bin_file == NULL) {
        log_ln("open %s fail", bin_file_name);
        return;
    }
    
    char line[512];
    Kline_item kline;
    //for(int i=0; i<20; i++){
    while (fscanf(csv_file, "%[^\n]%*c", line) != EOF) {
        //int ret=fscanf(csv_file, "%[^\n]%*c", line);
        if (line[0] != '#') {
            //log_ln("%d %s", ret, line);
            int sf = sscanf(line, "%lf,%lf,%lf,%lf,%lf,%lf,%d", &kline.open, &kline.close, &kline.high, &kline.low, &kline.amount, &kline.vol, &kline.count);
            fwrite(&kline, sizeof(Kline_item), 1, bin_file);
        }
    }
    fclose(csv_file);
    fclose(bin_file);
}

const char* KLINE_CHS[] = { "market.btcusdt.kline.15min", "market.btcusdt.kline.5min", "market.btcusdt.kline.1min",
                            "market.ethusdt.kline.15min", "market.ethusdt.kline.5min", "market.ethusdt.kline.1min"
                          };
const char* csv_file_dir = "..\\..\\..\\kline_csv";
const char* bin_file_dir = "..\\..\\..\\kline_bin";

void kline_csv_to_bin() {
    for (int i = 0; i < 6; i++) {
        char csv_file_name[512];
        snprintf(csv_file_name, sizeof(csv_file_name), "%s\\%s.csv", csv_file_dir, KLINE_CHS[i]);
        char bin_file_name[512];
        snprintf(bin_file_name, sizeof(bin_file_name), "%s\\%s.bin", bin_file_dir, KLINE_CHS[i]);
        
        kline_csv_to_bin(csv_file_name, bin_file_name);
    }
}

Kline_item* read_kline(const char* kline_ch, int start, int len) {
    char bin_file_name[512];
    snprintf(bin_file_name, sizeof(bin_file_name), "%s\\%s.bin", bin_file_dir, kline_ch);
    FILE* bin_file = fopen(bin_file_name, "rb");
    if (bin_file == NULL) {
        log_ln("open %s fail", bin_file_name);
        return NULL;
    }
    
    int kline_size = sizeof(Kline_item_sto) * len;
    
    // log_ln("start %d len %d kline_size %d", start, len, kline_size);
    Kline_item_sto* kline_sto = (Kline_item_sto*)malloc(kline_size);
    if (kline_sto == NULL) {
        log_ln("malloc %d fail", kline_size);
        return NULL;
    }
    
    if (fseek(bin_file, start * sizeof(Kline_item_sto), SEEK_SET) != 0) {
        log_ln("seek %s fail", bin_file_name);
        return NULL;
    }
    if (fread(kline_sto, kline_size, 1, bin_file) != 1) {
        log_ln("read %s fail", bin_file_name);
        return NULL;
    }
    fclose(bin_file);
    
    Kline_item* kline_buf = (Kline_item*)malloc(sizeof(Kline_item) * len);
    if (kline_buf == NULL) {
        log_ln("malloc kline_buf fail");
        return NULL;
    }
    for (int i = 0; i < len; i++) {
        kline_buf[i].open = kline_sto[i].open;
        kline_buf[i].close = kline_sto[i].close;
        kline_buf[i].high = kline_sto[i].high;
        kline_buf[i].low = kline_sto[i].low;
        kline_buf[i].amount = kline_sto[i].amount;
        kline_buf[i].vol = kline_sto[i].vol;
        kline_buf[i].count = kline_sto[i].count;
    }
    free(kline_sto);
    
    return kline_buf;
}

#define MA_NUM      60
#define FEE_RATE    0.0004

enum {
    STATE_IDLE = 0,
    STATE_LONG,
    STATE_SHORT
};

// #define NEED_LOG

__device__ D_FLOAT get_strategy_profit(const Kline_item* kline, int len, int fast_ma, int slow_ma, D_FLOAT open_diff, D_FLOAT close_diff, int leverage, D_FLOAT stop_loss) {
    int state = STATE_IDLE;
    D_FLOAT position_price = 0;
    D_FLOAT position_value = 1;
    D_FLOAT open_value = 0;
    
    D_FLOAT fast_ma_sum = 0;
    for (int i = MA_NUM - fast_ma; i < MA_NUM; i++) {
        fast_ma_sum += kline[i].close;
    }
    D_FLOAT slow_ma_sum = 0;
    for (int i = MA_NUM - slow_ma; i < MA_NUM; i++) {
        slow_ma_sum += kline[i].close;
    }
    
    D_FLOAT fee_left_ratio = 1 - leverage * FEE_RATE;
    
#ifdef NEED_LOG
    printf("get_strategy_profit len %d fast_ma %d slow_ma %d open_diff %.2f close_diff %.2f\n", len, fast_ma, slow_ma, open_diff, close_diff);
#endif
    
    for (int i = MA_NUM + 1; i < len; i++) {
        // printf("kline %d %.2f %.2f %.2f %.2f\n", i, kline[i].open, kline[i].close, kline[i].high, kline[i].low);
        fast_ma_sum -= kline[i - 1 - fast_ma].close;
        fast_ma_sum += kline[i - 1].close;
        D_FLOAT fast_ma_val = fast_ma_sum / fast_ma;
        
        slow_ma_sum -= kline[i - 1 - slow_ma].close;
        slow_ma_sum += kline[i - 1].close;
        D_FLOAT slow_ma_val = slow_ma_sum / slow_ma;
        
        D_FLOAT cur_price = (kline[i].open + kline[i].close + kline[i].high + kline[i].low) / 4;
        D_FLOAT diff = fast_ma_val - slow_ma_val;
        
        if (state == STATE_SHORT) {
            D_FLOAT cur_profit = (position_price - cur_price) / position_price;
            D_FLOAT leverage_profit = 1 + cur_profit * leverage;
            if (diff > kline[i - 1].close * close_diff) {
                state = STATE_IDLE;
                position_value = open_value * fee_left_ratio * leverage_profit;
                
#ifdef NEED_LOG
                printf("close short @ price %.2f profit %.3f position_value %.3f\n",
                       cur_price, cur_profit, position_value);
#endif
            } else if (((position_price - kline[i].high) / position_price) * leverage <= stop_loss) {
                state = STATE_IDLE;
                position_value = open_value * fee_left_ratio * (1 + stop_loss);
                
#ifdef NEED_LOG
                printf("stop loss short @ price %.2f position_value %.3f\n", cur_price, position_value);
#endif
            } else {
                position_value = open_value * leverage_profit;
            }
        } else if (state == STATE_LONG) {
            D_FLOAT cur_profit = (cur_price - position_price) / position_price;
            D_FLOAT leverage_profit = 1 + cur_profit * leverage;
            if (-diff > kline[i - 1].close * close_diff) {
                state = STATE_IDLE;
                position_value = open_value * fee_left_ratio * leverage_profit;
                
#ifdef NEED_LOG
                printf("close long @ price %.2f profit %.3f position_value %.3f\n",
                       cur_price, cur_profit, position_value);
#endif
            } else if (((kline[i].low - position_price) / position_price) * leverage <= stop_loss) {
                state = STATE_IDLE;
                position_value = open_value * fee_left_ratio * (1 + stop_loss);
                
#ifdef NEED_LOG
                printf("stop loss long @ price %.2f position_value %.3f\n", cur_price, position_value);
#endif
            } else {
                position_value = open_value * leverage_profit;
            }
        }
        
        if (state == STATE_IDLE) {
            if (((diff > 0) ? diff : (-diff)) > kline[i - 1].close * open_diff) {
                state = (diff > 0) ? STATE_LONG : STATE_SHORT;
                position_value = position_value * fee_left_ratio;
                open_value = position_value;
                position_price = cur_price;
                
                
#ifdef NEED_LOG
                printf("open %s @ price %.2f position_value %.3f\n",
                       (state == STATE_LONG) ? "long" : "short", cur_price, position_value);
#endif
            }
        }
    }
    
#ifdef NEED_LOG
    printf("final position_value %.4f\n", position_value);
#endif
    return position_value;
}

u32 start_time;

typedef struct {
    D_FLOAT profit;
    int fast_ma;
    int slow_ma;
    D_FLOAT open;
    D_FLOAT close;
} Strategy;
Strategy strategy;
int fast_ma;
int slow_ma;

// #define FAST_MA_MIN     1
// #define FAST_MA_MAX    (MA_NUM / 2)
#define FAST_MA_MIN     0
#define FAST_MA_MAX     27
#define FAST_MA_STEP    1
#define FAST_MA_NUM     ((FAST_MA_MAX-FAST_MA_MIN)/FAST_MA_STEP+1)

// #define SLOW_MA_MIN     3
// #define SLOW_MA_MAX     MA_NUM
#define SLOW_MA_MIN     43
#define SLOW_MA_MAX     45
#define SLOW_MA_STEP    1
#define SLOW_MA_NUM     ((SLOW_MA_MAX-SLOW_MA_MIN)/SLOW_MA_STEP+1)

#define OPEN_DIFF_MIN   0
#define OPEN_DIFF_MAX   0.1f
#define OPEN_DIFF_STEP  0.002f
#define OPEN_DIFF_NUM   (int)((OPEN_DIFF_MAX-OPEN_DIFF_MIN)/OPEN_DIFF_STEP)

#define CLOSE_DIFF_MIN  0
#define CLOSE_DIFF_MAX  0.1f
#define CLOSE_DIFF_STEP 0.002f
#define CLOSE_DIFF_NUM  ((CLOSE_DIFF_MAX-CLOSE_DIFF_MIN)/CLOSE_DIFF_STEP)

#define THREAD_TOTAL        FAST_MA_NUM*SLOW_MA_NUM*OPEN_DIFF_NUM
#define THREAD_PER_BLOCK    256
#define BLOCK_PER_GRID      (THREAD_TOTAL+THREAD_PER_BLOCK-1)/THREAD_PER_BLOCK


__global__ void training_kernel(const Kline_item* kline, int kline_len, Strategy* strategy) {
    int i = blockDim.x * blockIdx.x + threadIdx.x;
    if (i < THREAD_TOTAL) {
        int open_diff_index = i % OPEN_DIFF_NUM;
        D_FLOAT open_diff = open_diff_index * CLOSE_DIFF_STEP + CLOSE_DIFF_MIN;
        i /= OPEN_DIFF_NUM;
        
        int slow_ma = (i % SLOW_MA_NUM) * SLOW_MA_STEP + SLOW_MA_MIN;
        i /= SLOW_MA_NUM;
        
        int fast_ma = i * FAST_MA_STEP + FAST_MA_MIN;
        
        strategy = &strategy[i];
        strategy->profit = -1;
        
        open_diff = 0;
        slow_ma = 44;
        fast_ma = 27;
        
        for (D_FLOAT close_diff = CLOSE_DIFF_MIN; close_diff < CLOSE_DIFF_MAX; close_diff += CLOSE_DIFF_STEP) {
            // D_FLOAT close_diff = 0.004;
            D_FLOAT cur_profit = get_strategy_profit(kline, kline_len, fast_ma, slow_ma, open_diff, close_diff, 1, -0.1);
            if (cur_profit > strategy->profit) {
                strategy->profit = cur_profit;
                strategy->fast_ma = fast_ma;
                strategy->slow_ma = slow_ma;
                strategy->open = open_diff;
                strategy->close = close_diff;
            }
        }
    }
}


Strategy* training_ma_cross(Kline_item* target_kline, int kline_len) {
    cudaSetDevice(0);
    
    cudaError_t err = cudaSuccess;
    int kline_size = kline_len * sizeof(Kline_item);
    
    Kline_item* d_kline;
    err = cudaMalloc((void**)&d_kline, kline_size);
    if (err != cudaSuccess) {
        log_ln("Failed to allocate device kline (%s)!", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    err = cudaMemcpy(d_kline, target_kline, kline_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        log_ln("Failed to copy kline from host to device (%s)!", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    int strategy_size = THREAD_TOTAL * sizeof(Strategy);
    Strategy* d_strategy;
    err = cudaMalloc((void**)&d_strategy, strategy_size);
    if (err != cudaSuccess) {
        log_ln("Failed to allocate d_strategy (%s)!", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    
    log_ln("CUDA kernel launch with %d blocks of %d threads", BLOCK_PER_GRID, THREAD_PER_BLOCK);
    // training_kernel <<< BLOCK_PER_GRID, THREAD_PER_BLOCK>>> (d_kline, kline_len, d_strategy);
    training_kernel <<< 1, 1>>> (d_kline, kline_len, d_strategy);
    
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);
    float milliseconds = 0;
    cudaEventElapsedTime(&milliseconds, start, stop);
    log_ln("Kernel cost time %.3fms", milliseconds);
    
    log_ln("cudaThreadSynchronize %s", cudaGetErrorString(cudaThreadSynchronize()));
    
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        log_ln("Failed to launch kernel (%s)!", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    Strategy* h_strategy = (Strategy*)malloc(strategy_size);
    if (h_strategy == NULL) {
        log_ln("Failed to allocate h_strategy");
        exit(EXIT_FAILURE);
    }
    
    
    log_ln("Copy strategy from the CUDA device to the host memory");
    err = cudaMemcpy(h_strategy, d_strategy, strategy_size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        log_ln("Failed to copy strategy from device to host (%s)!", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
    
    cudaFree(d_kline);
    cudaFree(d_strategy);
    
    // memcpy(&strategy, &h_strategy[0], sizeof(Strategy));
    strategy.profit = -1;
    for (int i = 0; i < THREAD_TOTAL; i++) {
        if (h_strategy[i].profit > strategy.profit) {
            memcpy(&strategy, &h_strategy[i], sizeof(Strategy));
        }
    }
    
    log_ln("best strategy : fast %d slow %d open %.3f close %.3f profit %.4f", strategy.fast_ma, strategy.slow_ma,
           strategy.open, strategy.close, strategy.profit);
           
    return &strategy;
}

const char* log_dir = "..\\..\\log";
void set_log_file() {
    char log_file_path[512];
    snprintf(log_file_path, sizeof(log_file_path), "%s\\log_%s.txt", log_dir, _now_to_str());
    open_log_file(log_file_path);
}

void log_time_cost() {
    log_ln("start @ %s", time_to_str(start_time));
    log_ln("end   @ %s", now_to_str());
    u32 cost = time(0) - start_time;
    log_ln("cost %ds %02d:%02d:%02d", cost, cost / 3600, cost % 3600 / 60, cost % 60);
}


int main(void) {
    deviceQuery();
    // test_vector_add();
    
    // return 0;
    
    // kline_csv_to_bin();
    set_log_file();
    
    
    const char* kline_ch = "market.ethusdt.kline.1min";
    int kline_period = 60;
    u32 kline_start_time = str_to_time("2017-11-01 00:00:00");
    u32 kline_total_len = (str_to_time("2021-06-10 00:00:00") - kline_start_time) / kline_period;
    u32 test_start = (str_to_time("2021-06-08 00:00:00") - kline_start_time ) / kline_period;
    u32 test_end = (str_to_time("2021-06-10 00:00:00") - kline_start_time ) / kline_period;
    log_ln("kline_start_time %d kline_total_len %d test_start %d test_end %d", kline_start_time, kline_total_len, test_start, test_end);
    
    Kline_item* full_kline = read_kline(kline_ch, 0, kline_total_len);
    if (full_kline == NULL) {
        return -1;
    }
    
    start_time = time(0);
    training_ma_cross(&full_kline[test_start], test_end - test_start);
    log_time_cost();
    return 0;
}

