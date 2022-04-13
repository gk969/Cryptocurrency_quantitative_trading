#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctime>
#include "windows.h"

#include "../cpp_common/utils.h"

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

#include "kline_data.h"
#include "indicator.h"
#include "strategy.h"

#define CSV_FILE_DIR "..\\..\\..\\kline_csv"
#define BIN_FILE_DIR "..\\..\\..\\kline_bin"



