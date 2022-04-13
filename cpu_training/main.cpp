#include "common.h"


#define FEE_RATE    0.0004


int training_thread_num;
HANDLE* training_thread;
int* training_thread_id;
bool* training_thread_working;
HANDLE training_lock;


HANDLE training_sema;

u32 start_time;

void training_thread_func(LPVOID lpParameter) {
    int* id = (int*)lpParameter;
    
    while (true) {
        WaitForSingleObject(training_sema, INFINITE);
        // log_ln("thread %d work", *id);
        
        while (true) {
            //Sleep(500);
            
            WaitForSingleObject(training_lock, INFINITE);
            
            // training_thread_working[*id] = false;
            
            
            
            ReleaseMutex(training_lock);
            
            // if (is_end) {
                break;
            // }
            
            // char buf[64];
            // log_ln("thread %d training %s fast %d slow %d", *id, get_kline_time(training_kline, buf, sizeof(buf)), fast, slow);
            
            
            // log_ln("thread %d fast %d slow %d time %dms", *id, fast, slow, GetTickCount64()-start_time);
            
            WaitForSingleObject(training_lock, INFINITE);
            
            ReleaseMutex(training_lock);
        }
    }
}
/*
#define MIX_RAND_KLINE  true
Strategy* training_ma_cross(Kline_item* target_kline, int target_len, bool is_mix_rand_kline = false) {
    
    for (int i = 0; i < training_thread_num; i++) {
        training_thread_working[i] = true;
    }
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
            // log_ln("best strategy : fast %d slow %d open %.3f close %.3f profit %.4f", strategy.fast_ma, strategy.slow_ma,
                   // strategy.open, strategy.close, strategy.profit);
        }
        ReleaseMutex(training_lock);
        
        if (is_end) {
            break;
        }
    }
    
    return &strategy;
}
*/


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

void start_training_thread(){
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    log_ln("cpu core %d", si.dwNumberOfProcessors);
    
    // log_ln("RAND_MAX %d", RAND_MAX);
    // srand(GetTickCount64());
    
    training_thread_num = si.dwNumberOfProcessors / 2;
    // training_thread_num = 1;
    training_thread = (HANDLE*)malloc(training_thread_num * sizeof(HANDLE));
    training_thread_id = (int*)malloc(training_thread_num * sizeof(int));
    training_thread_working = (bool*)malloc(training_thread_num * sizeof(bool));
    if (training_thread == NULL || training_thread_id == NULL || training_thread_working == NULL) {
        return;
    }
    
    training_sema = CreateSemaphore(NULL, 0, training_thread_num, NULL);
    training_lock = CreateMutex(NULL, FALSE, NULL);
    
    for (int i = 0; i < training_thread_num; i++) {
        training_thread_id[i] = i;
        training_thread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)training_thread_func,
                                          &training_thread_id[i], 0, NULL);
    }
}

int main(int argc, char* argv[]) {
    indicator_test();
    return 0;
    
    // all_kline_csv_to_bin();
    // return 0;
    
    // set_log_file();
    
    start_time = time(0);
    
    log_time_cost();
    
    // close_log_file();
    
    return 0;
}
