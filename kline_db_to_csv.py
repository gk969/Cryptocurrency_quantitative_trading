import time
import pymongo
import utils


KLINE_START_TIME_STR = "2017-11-01 00:00:00"
KLINE_START_TIME = utils.get_time_num(KLINE_START_TIME_STR)
KLINE_END_TIME_STR = "2021-06-10 00:00:00"
KLINE_END_TIME = utils.get_time_num(KLINE_END_TIME_STR)

# KLINE_START_TIME 1509465600
print(f'KLINE_START_TIME {KLINE_START_TIME}')
exit()

ch = ["market.btcusdt.kline.15min", "market.btcusdt.kline.5min", "market.btcusdt.kline.1min",
      "market.ethusdt.kline.15min", "market.ethusdt.kline.5min", "market.ethusdt.kline.1min"]
period = [15 * 60, 5 * 60, 60, 15 * 60, 5 * 60, 60]
for i in range(len(ch)):
    KLINE_CH = ch[i]
    KLINE_PERIOD = period[i]
    KLINE_NUM = int((KLINE_END_TIME - KLINE_START_TIME) / KLINE_PERIOD)

    print(f'KLINE_END_TIME {KLINE_END_TIME}')
    print(f'KLINE_NUM {KLINE_NUM}')

    huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['huobi_data']
    kline_coll = huobi_db.get_collection(KLINE_CH)

    with open(f"{KLINE_CH}.csv", "wt") as out:
        print(f'# {KLINE_CH} from {KLINE_START_TIME_STR} to {KLINE_END_TIME_STR}', file=out)
        print('# open, close, high, low, amount, vol, count', file=out)
        for i in range(KLINE_START_TIME, KLINE_END_TIME, KLINE_PERIOD):
            item = kline_coll.find_one({'id': i})
            if item is not None:
                print(f'{item["open"]},{item["close"]},{item["high"]},{item["low"]},'
                      f'{item["amount"]},{item["vol"]},{item["count"]}', file=out)
            else:
                raise Exception(f'kline not exist @ {utils.get_time_str(i)}', i)
