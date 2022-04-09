import requests
import time
import asyncio
import websocket
import json
import gzip
import os
import pymongo
import numpy as np
import pandas as pd
import mplfinance as mpf
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import utils

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"

KLINE_CH = "market.ethusdt.kline.5min"
KLINE_PERIOD = 60 * 5
FEE_RATE = 0.0004
MA_NUM = 60

KLINE_START_TIME = "2021-04-10 00:00:00"
KLINE_START_TIME = utils.get_time_num(KLINE_START_TIME)
KLINE_END_TIME = "2021-05-10 00:00:00"
KLINE_END_TIME = utils.get_time_num(KLINE_END_TIME)
KLINE_NUM = int((KLINE_END_TIME - KLINE_START_TIME) / KLINE_PERIOD)

print(f'KLINE_START_TIME {KLINE_START_TIME} KLINE_END_TIME {KLINE_END_TIME}')

huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['huobi_data']
kline_coll = huobi_db.get_collection(KLINE_CH)

kline = {
    'open': np.zeros(KLINE_NUM),
    'close': np.zeros(KLINE_NUM),
    'high': np.zeros(KLINE_NUM),
    'low': np.zeros(KLINE_NUM),
    'volume': np.zeros(KLINE_NUM)
}
i = 0
for t in range(KLINE_START_TIME, KLINE_END_TIME, KLINE_PERIOD):
    item = kline_coll.find_one({'id': t})
    if item is not None:
        kline['open'][i] = item['open']
        kline['close'][i] = item['close']
        kline['high'][i] = item['high']
        kline['low'][i] = item['low']
        kline['volume'][i] = item['amount']
        i += 1
    else:
        print(f'kline not exist @ {utils.get_time_str(t)}')
        exit()

kline_ma = []
price_for_ma = pd.Series(kline['close'])
for i in range(MA_NUM):
    kline_ma.append(price_for_ma.rolling(i + 1).mean()[MA_NUM:].to_list())


def get_ma(window):
    window = int(window)
    if window < 1 or window > MA_NUM:
        raise Exception("Invalid window!", window)

    return kline_ma[window - 1]


for i in range(10):
    print("open %.2f close %.2f high %.2f low %.2f vol %.2f" %
          (kline['open'][i], kline['close'][i], kline['high'][i], kline['low'][i], kline['volume'][i]))

KLINE_MEMBER = ['open', 'close', 'high', 'low', 'volume']
for mem in KLINE_MEMBER:
    kline[mem] = kline[mem][MA_NUM:]


def get_fixed_invest_profit(price):
    rates = []
    buy_sum = 0
    for i in range(len(price)):
        buy_sum += 1 / price[i]
        rates.append(buy_sum * price[i] / (i + 1))

    return rates


STATE_LONG = 1
STATE_IDLE = 0
STATE_SHORT = -1


def get_strategy_profit(fast_ma, slow_ma, open_diff, close_diff, leverage, stop_loss, need_log=True):
    if leverage < 1:
        raise Exception("Invalid leverage!", leverage)

    if stop_loss >= 0:
        raise Exception("Invalid stop_loss!", stop_loss)

    if need_log:
        print(f'fast_ma len {len(fast_ma)} data {fast_ma[:5]}')
        print(f'slow_ma len {len(slow_ma)} data {slow_ma[:5]}')

    state = STATE_IDLE
    position_price = 0
    position_value = 1
    open_value = 0
    values = [1]
    states = [STATE_IDLE]
    for i in range(1, len(fast_ma)):
        cur_price = (kline['open'][i] + kline['close'][i] + kline['high'][i] + kline['low'][i]) / 4
        diff = fast_ma[i - 1] - slow_ma[i - 1]

        if state == STATE_SHORT:
            cur_profit = (position_price - cur_price) / position_price
            if diff > kline['close'][i - 1] * close_diff:
                state = STATE_IDLE
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + cur_profit * leverage)

                if need_log:
                    print('close short @ price %.2f profit %.3f position_value %.3f' %
                          (cur_price, cur_profit, position_value))
            elif ((position_price - kline['high'][i]) / position_price) * leverage <= stop_loss:
                state = STATE_IDLE
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + stop_loss)

                if need_log:
                    print('stop loss short @ price %.2f position_value %.3f' % (cur_price, position_value))
            else:
                position_value = open_value * (1 + cur_profit * leverage)

        elif state == STATE_LONG:
            cur_profit = (cur_price - position_price) / position_price
            if -diff > kline['close'][i - 1] * close_diff:
                state = STATE_IDLE
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + cur_profit * leverage)

                if need_log:
                    print('close long @ price %.2f profit %.3f position_value %.3f' %
                          (cur_price, cur_profit, position_value))
            elif ((kline['low'][i] - position_price) / position_price) * leverage <= stop_loss:
                state = STATE_IDLE
                position_value = open_value * (1 - leverage * FEE_RATE) * (1 + stop_loss)

                if need_log:
                    print('stop loss long @ price %.2f position_value %.3f' % (cur_price, position_value))
            else:
                position_value = open_value * (1 + cur_profit * leverage)

        if state == STATE_IDLE:
            if abs(diff) > kline['close'][i - 1] * open_diff:
                state = STATE_LONG if diff > 0 else STATE_SHORT
                position_value = position_value * (1 - leverage * FEE_RATE)
                open_value = position_value
                position_price = cur_price

                if need_log:
                    print('open %s @ price %.2f position_value %.3f' %
                          ('long' if state == STATE_LONG else 'short', cur_price, position_value))

        values.append(position_value)
        states.append(state)

    if need_log:
        print('final position_value %.3f' % position_value)
    return values, states


# get_strategy_profit(get_ma(5), get_ma(20), 0.01, 0.005, 1, -0.1, True)

def test_strategy():
    profit = -1
    best = {}
    fast = 9
    slow = 23

    print(f'fast {fast} slow {slow}')
    all_profits=[]
    for open in np.arange(0, 0.01, 0.002):
        sub_profits=[]
        for close in np.arange(0, 0.1, 0.002):
            profits, states = get_strategy_profit(get_ma(fast), get_ma(slow), open, close, 1, -0.1,
                                                  need_log=False)
            cur_profit=profits[len(profits) - 1]
            sub_profits.append(cur_profit)
            if cur_profit > profit:
                profit = profits[len(profits) - 1]
                best['fast'] = fast
                best['slow'] = slow
                best['open'] = open
                best['close'] = close
                print('better profit %.2f fast %d slow %d open %.3f close %.3f' %
                      (profit, fast, slow, open, close))
        all_profits.append(sub_profits)

    fig, ax = plt.subplots()

    for sub_profits in all_profits:
        ax.plot(sub_profits)
    plt.show()


test_strategy()


def plot_kline_profit(profit, states, fast_ma, slow_ma):
    states = [s * 0.2 + 1 for s in states]
    fixed_profit = get_fixed_invest_profit(kline['close'])
    base_profit = [1 for i in range(len(kline['open']))]
    apds = [
        mpf.make_addplot(fast_ma),
        mpf.make_addplot(slow_ma),
        mpf.make_addplot(fixed_profit, color='g'),
        mpf.make_addplot(base_profit, color='b', width=1.75),
        mpf.make_addplot(states, color='black', width=1.75),
        mpf.make_addplot(profit, color='r', width=1.75, secondary_y='auto'),
    ]
    kline['date'] = pd.date_range(utils.get_time_str(KLINE_START_TIME), periods=KLINE_NUM,
                                  freq=("%dmin" % (KLINE_PERIOD / 60)))[MA_NUM:]
    kline_frame = pd.DataFrame(kline)
    kline_frame.set_index('date', inplace=True)

    mc = mpf.make_marketcolors(up='g', down='r')
    s = mpf.make_mpf_style(base_mpf_style='default', marketcolors=mc)
    mpf.plot(kline_frame, type='candle', volume=True, datetime_format='%m-%d %H:%M',
             tight_layout=True, figscale=2, style=s, addplot=apds, returnfig=True)
    fm = plt.get_current_fig_manager()
    fm.window.showMaximized()
    plt.show()

# fast 9 slow 23 open 0.006 close 0.012 profit 2.3147
# fast_ma = get_ma(9)
# slow_ma = get_ma(23)
# profit, states = get_strategy_profit(fast_ma, slow_ma, 0.006, 0.012, 1, -0.1, True)
# plot_kline_profit(profit, states, fast_ma, slow_ma)
