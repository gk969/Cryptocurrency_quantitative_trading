#include "common.h"
#include "kline_data.h"


#define MA_NUM      30
#define FEE_RATE    0.0004

enum {
    STATE_IDLE = 0,
    STATE_LONG,
    STATE_SHORT
};

#define  MIN_POSITION 0.1

// #define NEED_LOG

D_FLOAT get_strategy_profit(Kline_item* kline, int len, int fast_ma, int slow_ma, D_FLOAT open_diff, D_FLOAT close_diff, int leverage, D_FLOAT stop_loss) {
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
    
    D_FLOAT cur_profit;
    for (int i = MA_NUM + 1; i < len; i++) {
        fast_ma_sum -= kline[i - 1 - fast_ma].close;
        fast_ma_sum += kline[i - 1].close;
        D_FLOAT fast_ma_val = fast_ma_sum / fast_ma;
        
        slow_ma_sum -= kline[i - 1 - slow_ma].close;
        slow_ma_sum += kline[i - 1].close;
        D_FLOAT slow_ma_val = slow_ma_sum / slow_ma;
        
        D_FLOAT cur_price = (kline[i].open + kline[i].close + kline[i].high + kline[i].low) / 4;
        D_FLOAT diff = fast_ma_val - slow_ma_val;
        
        if (state == STATE_SHORT) {
            cur_profit = (position_price - cur_price) / position_price;
            if (diff > kline[i - 1].close * close_diff) {
                state = STATE_IDLE;
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + cur_profit * leverage);
                
#ifdef NEED_LOG
                log_ln("close short @ price %.2f profit %.3f position_value %.3f",
                       cur_price, cur_profit, position_value);
#endif
            } else if (((position_price - kline[i].high) / position_price) * leverage <= stop_loss) {
                state = STATE_IDLE;
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + stop_loss);
                
#ifdef NEED_LOG
                log_ln("stop loss short @ price %.2f position_value %.3f", cur_price, position_value);
#endif
            } else {
                position_value = open_value * (1 + cur_profit * leverage);
            }
        } else if (state == STATE_LONG) {
            cur_profit = (cur_price - position_price) / position_price;
            if (-diff > kline[i - 1].close * close_diff) {
                state = STATE_IDLE;
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + cur_profit * leverage);
                
#ifdef NEED_LOG
                log_ln("close long @ price %.2f profit %.3f position_value %.3f",
                       cur_price, cur_profit, position_value);
#endif
            } else if (((kline[i].low - position_price) / position_price) * leverage <= stop_loss) {
                state = STATE_IDLE;
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + stop_loss);
                
#ifdef NEED_LOG
                log_ln("stop loss long @ price %.2f position_value %.3f", cur_price, position_value);
#endif
            } else {
                position_value = open_value * (1 + cur_profit * leverage);
            }
        }
        
        if (state == STATE_IDLE) {
            if (((diff > 0) ? diff : (-diff)) > kline[i - 1].close * open_diff) {
                state = (diff > 0) ? STATE_LONG : STATE_SHORT;
                position_value = position_value * (1 - leverage * FEE_RATE);
                open_value = position_value;
                position_price = cur_price;
                
                
#ifdef NEED_LOG
                log_ln("open %s @ price %.2f position_value %.3f",
                       (state == STATE_LONG) ? "long" : "short", cur_price, position_value);
#endif
            }
        }
        
        if (position_value < MIN_POSITION) {
            if (state != STATE_IDLE) {
                position_value *= (1 - leverage * FEE_RATE);
            }
            break;
        }
    }
    
#ifdef NEED_LOG
    log_ln("final position_value %.4f", position_value);
#endif
    return position_value;
}

int training_thread_num;
HANDLE* training_thread;
int* training_thread_id;
bool* training_thread_working;
HANDLE training_lock;


#define KLINE_CH        "market.ethusdt.kline.15min"
#define KLINE_PERIOD    (60 * 15)


u32 full_kline_len;
Kline_item* full_kline;
u32 kline_start_time;

Kline_item* rand_kline;
int rand_kline_num;
int rand_kline_step;


HANDLE training_sema;

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

#define MA_STEP     2
#define DIFF_STEP   0.002

#define FAST_END_MA  (MA_NUM / 2)

int training_kline_len;
Kline_item* training_kline;

bool training_mix_rand_kline = false;

char* get_kline_time(Kline_item* kline, char* buf, int buf_size) {
    u32 kline_time = kline_start_time + ((u32)kline - (u32)full_kline) / sizeof(Kline_item) * KLINE_PERIOD;
    time_to_str(kline_time, buf, buf_size);
    return buf;
}

void training_thread_func(LPVOID lpParameter) {
    int* id = (int*)lpParameter;
    
    while (true) {
        WaitForSingleObject(training_sema, INFINITE);
        // log_ln("thread %d work", *id);
        
        while (true) {
            //Sleep(500);
            
            WaitForSingleObject(training_lock, INFINITE);
            int fast = fast_ma;
            int slow = slow_ma;
            bool is_end = fast_ma >= FAST_END_MA;
            if (is_end) {
                training_thread_working[*id] = false;
            } else {
                slow_ma += MA_STEP;
                if (slow_ma >= MA_NUM) {
                    fast_ma += MA_STEP;
                    slow_ma = fast_ma * 3 / 2;
                }
            }
            
            
            ReleaseMutex(training_lock);
            
            if (is_end) {
                break;
            }
            
            // char buf[64];
            // log_ln("thread %d training %s fast %d slow %d", *id, get_kline_time(training_kline, buf, sizeof(buf)), fast, slow);
            
            // u64 start_time = GetTickCount64();
            Strategy cur_strategy;
            cur_strategy.profit = -1;
            for (D_FLOAT open = 0; open < 0.1; open += DIFF_STEP) {
                for (D_FLOAT close = 0; close < 0.1; close += DIFF_STEP) {
                    // D_FLOAT open=0;
                    // D_FLOAT close=0;
                    
                    D_FLOAT cur_profit = get_strategy_profit(training_kline, training_kline_len, fast, slow, open, close, 1, -0.1);
                    // log_ln("thread %d open %.3f close %.3f profit %.3f", *id, open, close, cur_profit);
                    if (training_mix_rand_kline) {
                        for (int i = 0; i < rand_kline_num; i++) {
                            int rand_part = (((u64)training_kline - (u64)rand_kline) / sizeof(Kline_item) - rand_kline_step) / rand_kline_num;
                            int rand_ofs = rand_part * i + rand() % rand_part;
                            Kline_item* kline = &rand_kline[rand_ofs];
                            
                            D_FLOAT profit = get_strategy_profit(kline, rand_kline_step, fast, slow, open, close, 1, -0.1);
                            // log_ln("thread %d training rand %d %s profit %.3f", *id, rand_ofs, get_kline_time(kline, buf, sizeof(buf)), profit);
                            cur_profit *= profit;
                        }
                    }
                    if (cur_profit > cur_strategy.profit) {
                        // log_ln("thread %d better profit %.3f", cur_profit);
                        cur_strategy.profit = cur_profit;
                        cur_strategy.fast_ma = fast;
                        cur_strategy.slow_ma = slow;
                        cur_strategy.open = open;
                        cur_strategy.close = close;
                    }
                }
            }
            
            // log_ln("thread %d fast %d slow %d time %dms", *id, fast, slow, GetTickCount64()-start_time);
            
            WaitForSingleObject(training_lock, INFINITE);
            if (cur_strategy.profit > strategy.profit) {
                log_ln("thread %d better profit %.4f fast %d slow %d open %.3f close %.3f", *id,
                       cur_strategy.profit, cur_strategy.fast_ma, cur_strategy.slow_ma, cur_strategy.open, cur_strategy.close);
                strategy = cur_strategy;
            }
            ReleaseMutex(training_lock);
        }
    }
}

#define MIX_RAND_KLINE  true
Strategy* training_ma_cross(Kline_item* target_kline, int target_len, bool is_mix_rand_kline = false) {
    char training_time[64];
    get_kline_time(target_kline, training_time, sizeof(training_time));
    log_ln("training_ma_cross @ %s %.1f day", training_time, (float)((target_len * KLINE_PERIOD) / 3600) / 24);
    
    training_kline_len = target_len;
    training_kline = target_kline;
    training_mix_rand_kline = is_mix_rand_kline;
    
    
    //for (int i = 0; i < 10; i++) {
    //    Kline_item* item = (Kline_item*)(((char*)kline) + i * sizeof(Kline_item));
    //    log_ln("cnt %d open %.2f close %.2f high %.2f low %.2f vol %.2f", item->count, item->open, item->close, item->high, item->low, item->vol);
    //}
    
    // best strategy : fast 27 slow 44 open 0.000 close 0.004 profit 1.2385
    // get_strategy_profit(kline, training_kline_len, 27, 44, 0, 0.004, 1, -0.1);
    // return NULL;
    
    strategy.profit = -1;
    for (int i = 0; i < training_thread_num; i++) {
        training_thread_working[i] = true;
    }
    fast_ma = 1;
    slow_ma = 3;
    ReleaseSemaphore(training_sema, training_thread_num, NULL);
    
    while (true) {
        //log_ln("thread main work");
        Sleep(50);
        
        WaitForSingleObject(training_lock, INFINITE);
        bool is_end = true;
        for (int i = 0; i < training_thread_num; i++) {
            if (training_thread_working[i]) {
                is_end = false;
                break;
            }
        }
        if (is_end) {
            log_ln("best strategy : fast %d slow %d open %.3f close %.3f profit %.4f", strategy.fast_ma, strategy.slow_ma,
                   strategy.open, strategy.close, strategy.profit);
        }
        ReleaseMutex(training_lock);
        
        if (is_end) {
            break;
        }
    }
    
    return &strategy;
}


void test_training_ma_cross(u32 training_start, u32 training_end) {
    log_ln("test_training_ma_cross training_start %d training_end %d", training_start, training_end);
    
    u32 best_training_hour;
    u32 best_test_hour;
    D_FLOAT max_profit = -1;
    int hour_step = 24;
    // rand_kline_step = (2*24+training_hour) / rand_kline_num * 3600 / KLINE_PERIOD + MA_NUM;
    // for (int test_hour = 10 * hour_step; test_hour < 40 * hour_step; test_hour += hour_step * 10) {
    // for (int training_hour = test_hour; training_hour < 60 * hour_step; training_hour += hour_step * 10) {
    int training_hour = 24 * 15;
    int test_hour = 24 * 10;
    log_ln("training_hour %d test_hour %d", training_hour, test_hour);
    u32 training_step = training_hour * 3600 / KLINE_PERIOD;
    u32 test_step = test_hour * 3600 / KLINE_PERIOD;
    
    
    
    u32 total_len = training_end - training_start;
    if (total_len % test_step != 0) {
        training_start -= test_step - total_len % test_step;
        log_ln("training_start %d ", training_start);
    }
    
    D_FLOAT test_profit = 1;
    for (u32 i = training_start; i < training_end; i += test_step) {
        // u32 training = i - training_step - MA_NUM;
        u32 training = i - 3 * 365 * 24 * 3600 / KLINE_PERIOD - MA_NUM;
        Strategy cur_strategy = *training_ma_cross(&full_kline[training], training_step + MA_NUM);
        D_FLOAT training_profit = get_strategy_profit(&full_kline[training], training_step + MA_NUM, cur_strategy.fast_ma, cur_strategy.slow_ma, cur_strategy.open, cur_strategy.close, 1, -0.1);
        D_FLOAT profit = get_strategy_profit(&full_kline[i - MA_NUM], test_step + MA_NUM, cur_strategy.fast_ma, cur_strategy.slow_ma, cur_strategy.open, cur_strategy.close, 1, -0.1);
        test_profit *= profit;
        
        char training_time[64];
        get_kline_time(&full_kline[i - training_step], training_time, sizeof(training_time));
        char test_time[64];
        get_kline_time(&full_kline[i], test_time, sizeof(test_time));
        log_ln("training %s test %s strategy training_profit %.4f %.4f profit %.4f test_profit %.4f", training_time, test_time, cur_strategy.profit, training_profit, profit, test_profit);
    }
    
    log_ln("training_hour %d test_hour %d test end", training_hour, test_hour);
    
    if (test_profit > max_profit) {
        max_profit = test_profit;
        best_training_hour = training_hour;
        best_test_hour = test_hour;
        log_ln("better @ training_hour %d test_hour %d max_profit %.4f", training_hour, test_hour, max_profit);
    }
    // }
    // }
    log_ln("best @ training_hour %d test_hour %d profit %.3f", best_training_hour, best_test_hour, max_profit);
}


const char* log_dir = "..\\..\\log";
void set_log_file() {
    char log_file_path[512];
    snprintf(log_file_path, sizeof(log_file_path), "%s\\log %s.txt", log_dir, _now_to_str());
    open_log_file(log_file_path);
}

void log_time_cost() {
    log_ln("start @ %s", time_to_str(start_time));
    log_ln("end   @ %s", now_to_str());
    u32 cost = time(0) - start_time;
    log_ln("cost %ds %02d:%02d:%02d", cost, cost / 3600, cost % 3600 / 60, cost % 60);
}


int main(int argc, char* argv[]) {
    indicator_test();
    return 0;
    
    // all_kline_csv_to_bin();
    // return 0;
    
    set_log_file();
    
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    log_ln("cpu core %d", si.dwNumberOfProcessors);
    
    log_ln("RAND_MAX %d", RAND_MAX);
    srand(GetTickCount64());
    
    training_thread_num = si.dwNumberOfProcessors / 2;
    // training_thread_num = 1;
    training_thread = (HANDLE*)malloc(training_thread_num * sizeof(HANDLE));
    training_thread_id = (int*)malloc(training_thread_num * sizeof(int));
    training_thread_working = (bool*)malloc(training_thread_num * sizeof(bool));
    if (training_thread == NULL || training_thread_id == NULL || training_thread_working == NULL) {
        return -1;
    }
    
    training_sema = CreateSemaphore(NULL, 0, training_thread_num, NULL);
    training_lock = CreateMutex(NULL, FALSE, NULL);
    
    for (int i = 0; i < training_thread_num; i++) {
        training_thread_id[i] = i;
        training_thread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)training_thread_func,
                                          &training_thread_id[i], 0, NULL);
    }
    
    
    kline_start_time = str_to_time("2017-11-01 00:00:00");
    full_kline_len = (str_to_time("2021-06-10 00:00:00") - kline_start_time) / KLINE_PERIOD;
    full_kline = read_kline(KLINE_CH, 0, full_kline_len);
    if (full_kline == NULL) {
        return -1;
    }
    
    
    
    u32 training_start = (str_to_time("2017-12-00 00:00:00") - kline_start_time ) / KLINE_PERIOD;
    u32 training_end = (str_to_time("2018-08-00 00:00:00") - kline_start_time ) / KLINE_PERIOD;
    log_ln("%s kline_start_time %d full_kline_len %d training_start %d training_end %d", KLINE_CH, kline_start_time, full_kline_len, training_start, training_end);
    
    u32 test_start = (str_to_time("2020-12-01 00:00:00") - kline_start_time ) / KLINE_PERIOD;
    u32 test_end = (str_to_time("2021-06-01 00:00:00") - kline_start_time ) / KLINE_PERIOD;
    
    
    start_time = time(0);
    
    
    D_FLOAT test_profit = get_strategy_profit(&full_kline[test_start], test_end - test_start, 5, 10, 0.0004, 0.0002, 1, -0.1);
    log_ln("test_profit %.3f", test_profit);
    
    // u32 rand_start_time = str_to_time("2017-12-01 00:00:00");
    // rand_kline_num = 30;
    // rand_kline_step = 1 * 24 * 3600 / KLINE_PERIOD + MA_NUM;
    // rand_kline = &full_kline[(rand_start_time - kline_start_time) / KLINE_PERIOD];
    
    // Strategy test_best_strategy = *training_ma_cross(&full_kline[training_start], training_end - training_start);
    // D_FLOAT test_profit = get_strategy_profit(&full_kline[test_start], test_end - test_start, test_best_strategy.fast_ma, test_best_strategy.slow_ma, test_best_strategy.open, test_best_strategy.close, 1, -0.1);
    // log_ln("test_profit %.3f", test_profit);
    
    // test_training_ma_cross(training_start, training_end);
    
    // log_ln("test_best_strategy fast %d slow %d open %.3f close %.3f profit %.4f", test_best_strategy.fast_ma, test_best_strategy.slow_ma,
    // test_best_strategy.open, test_best_strategy.close, test_best_strategy.profit);
    
    log_time_cost();
    
    close_log_file();
    
    // getchar();
    return 0;
}
