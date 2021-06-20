import requests
import time
import asyncio
import websocket
import json
import gzip
import pymongo
import numpy as np
import pandas as pd
import mplfinance as mpf
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def get_time_str(time_num):
    return time.strftime(TIME_FORMAT, time.localtime(time_num))


def get_time_num(time_str):
    return int(time.mktime(time.strptime(time_str, TIME_FORMAT)))


def get_fixed_investment_rate(close_price):
    buy_sum = 0
    rates = []
    for i in range(len(close_price)):
        buy_sum += 1 / close_price[i]
        rates.append(buy_sum * close_price[i] / (i + 1))

    return rates


KLINE_CH = "market.ethusdt.kline.15min"
KLINE_PERIOD = 15 * 60

KLINE_START_TIME = "2021-06-05 00:00:00"
KLINE_START_TIME = get_time_num(KLINE_START_TIME)
KLINE_END_TIME = "2021-06-13 12:00:00"
KLINE_END_TIME = get_time_num(KLINE_END_TIME)
KLINE_NUM = int((KLINE_END_TIME - KLINE_START_TIME) / KLINE_PERIOD)

huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['huobi_data']
kline_coll = huobi_db.get_collection(KLINE_CH)

kline_open = []
kline_close = []
kline_high = []
kline_low = []
kline_amount = []
for i in range(KLINE_START_TIME, KLINE_END_TIME, KLINE_PERIOD):
    item = kline_coll.find_one({'id': i})
    if item is not None:
        kline_open.append(item['open'])
        kline_close.append(item['close'])
        kline_high.append(item['high'])
        kline_low.append(item['low'])
        kline_amount.append(item['amount'])
    else:
        print(f'kline not exist @ {get_time_str(i)}')
        exit()

fixed_rate = get_fixed_investment_rate(kline_close)
base_rate = [1 for i in range(KLINE_NUM)]
apds = [
    mpf.make_addplot(fixed_rate, color='g', secondary_y='auto'),
    mpf.make_addplot(base_rate, color='b', width=1.75)
]

time_index = pd.date_range(get_time_str(KLINE_START_TIME), periods=KLINE_NUM, freq="15min")
kline_frame = pd.DataFrame({
    'open': kline_open,
    'high': kline_high,
    'low': kline_low,
    'close': kline_close,
    'volume': kline_amount,
    'date': time_index
})
kline_frame.set_index('date', inplace=True)

mc = mpf.make_marketcolors(up='g', down='r')
s = mpf.make_mpf_style(base_mpf_style='default', marketcolors=mc)
mpf.plot(kline_frame, type='candle', mav=(5, 10, 30), volume=True, datetime_format='%m-%d %H:%M',
         tight_layout=True, figscale=2, style=s, addplot=apds, returnfig=True)
fm = plt.get_current_fig_manager()
fm.window.showMaximized()
plt.show()
