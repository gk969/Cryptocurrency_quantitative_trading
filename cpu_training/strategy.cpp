#include "common.h"

enum{
    OPERATE_NONE=0,
    OPERATE_OPEN_LONG,
    OPERATE_OPEN_SHORT,
    OPERATE_CLOSE,
};

D_FLOAT* get_value_by_operate(Kline_item* kline, int len, u8* operate){
    u8 cur_operate=OPERATE_NONE;
    D_FLOAT position_price = 0;
    D_FLOAT position_value = 1;
    D_FLOAT open_value = 0;
    
}

D_FLOAT* test_ema_cross(Kline_item* kline, int len, int fast_ema, int slow_ema){
    
}

D_FLOAT* test_macd(Kline_item* kline, int len, int fast_ema, int slow_ema, int signal_ema){
    Indicator_MACD* macd=new Indicator_MACD(kline, len, fast_ema. slow_ema, signal_ema);
    
    u8* operate=(u8*)malloc(len*sizeof(u8));
    if(operate==NULL){
        log_ln("test_macd malloc operate fail");
        exit(0);
        return;
    }
    
    int valid_start=macd->get_valid_start();
    for(int i=0; i<valid_start; i++){
        operate[i]=OPERATE_NONE;
    }
    
    for(int i=valid_start+1; i<len; i++){
        D_FLOAT cur_macd=macd.get(i);
        if(cur_macd*macd.get(i-1)>0){
            operate[i]=OPERATE_NONE;
        }else if(cur_macd>0){
            operate[i]=OPERATE_OPEN_LONG;
        }else{
            operate[i]=OPERATE_OPEN_SHORT;
        }
    }
    
    
}

