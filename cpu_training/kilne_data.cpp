#include "kline_data.h"


void kline_csv_to_bin(int kline_period, const char* csv_file_name, const char* bin_file_name) {
    log_ln("kline_csv_to_bin %s -> %s", csv_file_name, csv_file_name);

    FILE* csv_file = fopen(csv_file_name, "r");
    if (csv_file == NULL) {
        log_ln("open %s fail", csv_file_name);
        exit(EXIT_FAILURE);
        return;
    }

    FILE* bin_file = fopen(bin_file_name, "wb");
    if (bin_file == NULL) {
        log_ln("open %s fail", bin_file_name);
        exit(EXIT_FAILURE);
        return;
    }

    char line[512];
    Kline_item_sto kline;
    int kline_lines = 0;
    int kline_lines_in_file = 0;
    //for(int i=0; i<20; i++){
    while (fscanf(csv_file, "%[^\n]%*c", line) != EOF) {
        //int ret=fscanf(csv_file, "%[^\n]%*c", line);
        if (line[0] != '#') {
            //log_ln("%d %s", ret, line);
            if (kline_lines == 0) {
                log_ln("read kline_lines fail");
                exit(EXIT_FAILURE);
                return;
            }
            int sf = sscanf(line, "%lf,%lf,%lf,%lf,%lf,%lf,%d", &kline.open, &kline.close, &kline.high, &kline.low, &kline.amount, &kline.vol, &kline.count);
            fwrite(&kline, sizeof(Kline_item_sto), 1, bin_file);
            kline_lines_in_file++;
        }
        else if (strstr(line, "from") != NULL && strstr(line, "to") != NULL) {
            log_ln(line);
            time_t start_time = str_to_time(strstr(line, "from") + 5);
            time_t end_time = str_to_time(strstr(line, "to") + 3);
            kline_lines = (end_time - start_time) / kline_period;
            log_ln("start_time %d end_time %d kline_lines %d", start_time, end_time, kline_lines);
        }
    }
    fclose(csv_file);
    fclose(bin_file);

    log_ln("kline_lines_in_file %d kline_lines %d", kline_lines_in_file, kline_lines);
    if (kline_lines_in_file != kline_lines) {
        exit(EXIT_FAILURE);
    }
}

typedef struct {
    const char* name;
    int period;
} Kline_period;

Kline_period KLINE_PERIODS[] = {
    {"1min", 60},
    {"5min", 5 * 60},
    {"15min", 15 * 60},
    {"30min", 30 * 60},
    {"60min", 60 * 60},
    {"4hour", 4 * 3600},
    {"1day", 24 * 3600},
};
#define PERIOD_NUM  (sizeof(KLINE_PERIODS)/sizeof(Kline_period))

// const char* KLINE_COIN[] = { "btc"};
const char* KLINE_COIN[] = { "btc", "eth", "doge", "ltc", "xrp", "ada", "uni", "dot", "sol", "shib", "etc"};
#define KLINE_NUM   (sizeof(KLINE_COIN)/sizeof(char*))


void all_kline_csv_to_bin() {
    for (int k = 0; k < KLINE_NUM; k++) {
        for (int p = 0; p < PERIOD_NUM; p++) {
            char kline_name[64];
            snprintf(kline_name, sizeof(kline_name), "market.%susdt.kline.%s", KLINE_COIN[k], KLINE_PERIODS[p].name);

            char csv_file_name[512];
            snprintf(csv_file_name, sizeof(csv_file_name), "%s\\%s.csv", CSV_FILE_DIR, kline_name);
            char bin_file_name[512];
            snprintf(bin_file_name, sizeof(bin_file_name), "%s\\%s.bin", BIN_FILE_DIR, kline_name);

            kline_csv_to_bin(KLINE_PERIODS[p].period, csv_file_name, bin_file_name);
        }
    }
}


Kline_item* read_kline(const char* kline_ch, int start, int len) {
    char bin_file_name[512];
    snprintf(bin_file_name, sizeof(bin_file_name), "%s\\%s.bin", BIN_FILE_DIR, kline_ch);
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
