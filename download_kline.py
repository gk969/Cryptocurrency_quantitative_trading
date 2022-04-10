import sys
import os

import requests
import time
import asyncio
import websocket
import json
import gzip
from enum import Enum
import pymongo
from utils import *

State = Enum('State', ('IDLE', 'SUB', 'REQ', 'END'))
state = State.IDLE
MAX_KLINE_PER_REQ = 720
REQ_KLINE_START = "2017-01-01 00:00:00"
REQ_KLINE_START = get_time_num(REQ_KLINE_START)
REQ_KLINE_END = "2022-03-24 00:00:00"
REQ_KLINE_END = get_time_num(REQ_KLINE_END)

real_kline_start = False
coin_start_time = 0

kline_start_time = 0
kline_req_time = 0
kline_send_time = time.time()


req_success_cnt = 0
req_fail_cnt = 0

kline_coll = {}
kline_sub = {}
last_kline = {}
kline_ch = ''
kline_period = 0

CHECK_COIN_START = 0

TIME_PERIODS = [
    {'nm': '1day', 'tm': 24 * 3600}, # check kline start
    {'nm': '1min', 'tm': 60},
    {'nm': '5min', 'tm': 5 * 60},
    {'nm': '15min', 'tm': 15 * 60},
    {'nm': '30min', 'tm': 30 * 60},
    {'nm': '60min', 'tm': 60 * 60},
    {'nm': '4hour', 'tm': 4 * 3600},
    {'nm': '1day', 'tm': 24 * 3600},
]

# COIN_NAMES = ['eth']
COIN_NAMES = ['btc', 'eth', 'doge', 'ltc', 'xrp', 'ada', 'uni', 'dot', 'sol', 'shib', 'etc']

coin_idx = 0
period_idx = 0

huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['huobi_data']

def start_req_kline():
    global last_kline, kline_ch, kline_period, kline_coll, kline_sub, real_kline_start, kline_start_time
    last_kline = {
        "id": 0,
        "open": 0,
        "close": 0,
        "low": 0,
        "high": 0,
        "amount": 0,
        "vol": 0,
        "count": 0
    }

    kline_ch = f"market.{COIN_NAMES[coin_idx]}usdt.kline.{TIME_PERIODS[period_idx]['nm']}"
    kline_period = TIME_PERIODS[period_idx]['tm']

    kline_sub = {
        "sub": kline_ch,
        "id": "id1"
    }

    print(f'start kline {kline_ch}')

    kline_coll = huobi_db.get_collection(kline_ch)

    kline_start_time = REQ_KLINE_START if period_idx == CHECK_COIN_START else coin_start_time
    real_kline_start = False

    if is_kline_complete():
        switch_next_kline()
        return

    start_web_socket()


def kline_to_csv():
    with open(os.path.join('kline_csv', f"{kline_ch}.csv"), "wt") as out:
        print(f'# {kline_ch} from {get_time_str(kline_start_time)} to {get_time_str(REQ_KLINE_END)}', file=out)
        print('# open, close, high, low, amount, vol, count', file=out)
        for i in range(kline_start_time, REQ_KLINE_END, kline_period):
            item = kline_coll.find_one({'_id': i})
            if item is not None:
                print(f'*** write csv @ {get_time_str(i)} d {i % (24 * 3600)}')
                if (i % (24 * 3600)) == 0:
                    print(f'write csv @ {get_time_str(i)}')
                print(f'{item["open"]},{item["close"]},{item["high"]},{item["low"]},'
                      f'{item["amount"]},{item["vol"]},{item["count"]}', file=out)
            else:
                raise Exception(f'kline not exist @ {get_time_str(i)}', i)


def switch_next_kline():
    print(f'switch_next_kline')

    global period_idx, coin_idx, coin_start_time
    period_idx += 1
    if period_idx >= len(TIME_PERIODS):
        period_idx = 0
        coin_start_time = 0
        coin_idx += 1
        if coin_idx >= len(COIN_NAMES):
            print('kline req ALL complete')
            exit()

    start_req_kline()


def is_kline_complete():
    global kline_req_time, kline_start_time, real_kline_start, coin_start_time
    if kline_coll.count_documents({}) != 0:
        kline_start_time = kline_coll.find_one(sort=[("_id", pymongo.ASCENDING)])['_id']
        real_kline_start = True
        if coin_start_time == 0:
            coin_start_time = kline_start_time
        print(f'kline_start_time {get_time_str(kline_start_time)}')
        max_time_in_db = kline_coll.find_one(sort=[("_id", pymongo.DESCENDING)])['_id']
        kline_req_time = max_time_in_db + kline_period
        if kline_req_time >= REQ_KLINE_END:
            print('all kline time exist in db')
            return True
    else:
        kline_req_time = kline_start_time
    print('kline_req_time %s' % get_time_str(kline_req_time))
    return False


def save_kline_data(data):
    for item in data:
        item['_id'] = item.pop('id')
        item['time'] = get_time_str(item['_id'])
        if kline_coll.count_documents({'_id': item['_id']}) == 0:
            kline_coll.insert_one(item)
        else:
            print(f'list @ %d exist' % item['_id'])


def send_kline_req(ws):
    global kline_send_time
    to_time = min(REQ_KLINE_END - kline_period, kline_req_time +
                  kline_period * (MAX_KLINE_PER_REQ - 1))
    kline_req = {
        "req": kline_ch,
        "id": "req " + str(kline_req_time),
        "from": kline_req_time,
        "to": to_time
    }
    ws.send(json.dumps(kline_req))
    cur_time = time.time()
    print('req kline %s %.2fs %s %d' % (
        kline_ch, cur_time - kline_send_time, get_time_str(kline_req_time), kline_req_time))
    kline_send_time = cur_time


# {
#     "id": "req 1623470400",
#     "status": "ok",
#     "ts": 1623515792624,
#     "rep": "market.ethusdt.kline.1min",
#     "data": [
#         {
#             "id": 1623470400,
#             "open": 2300.51,
#             "close": 2306.87,
#             "low": 2296.58,
#             "high": 2307.39,
#             "amount": 559.162769977232,
#             "vol": 1287419.0268039096,
#             "count": 1268
#         }
#     ]
# }

def get_kline_item_from_list(tm_id, data):
    global last_kline
    for item in data:
        if (item.get('id') == tm_id and 'open' in item and 'close' in item and 'low' in item and 'high' in item and
                'amount' in item and 'vol' in item and 'count' in item):
            last_kline = item.copy()
            return item

    print(f"kline lost @ {tm_id} {get_time_str(tm_id)}")
    last_kline['id'] = tm_id
    last_kline['open'] = last_kline['close']
    last_kline['low'] = last_kline['close']
    last_kline['high'] = last_kline['close']
    last_kline['amount'] = 0
    last_kline['vol'] = 0
    last_kline['count'] = 0
    return last_kline.copy()


def fix_kline_data(data, pack_len):
    print(f'fix_kline_data pack_len {pack_len}')
    ret_list = []
    cur_pack_len = min(pack_len, int((REQ_KLINE_END - kline_req_time) / kline_period))
    for i in range(cur_pack_len):
        ret_list.append(get_kline_item_from_list(kline_req_time + i * kline_period, data))
    return ret_list


def on_message(ws, msg):
    global state, kline_req_time
    msg = gzip.decompress(msg).decode('utf8')
    msg = json.loads(msg)
    # print(f"state {state} on_message {msg}")
    if "ping" in msg:
        pong = {"pong": msg['ping']}
        ws.send(json.dumps(pong))

    if state == State.SUB:
        if msg.get('id') == kline_sub['id'] and msg.get('subbed') == kline_sub['sub'] and msg.get('status') == 'ok':
            state = State.REQ
            return
        else:
            print(f"sub kline error : {msg}")
        ws.send(json.dumps(kline_sub))

    global req_fail_cnt
    if state == State.REQ:
        if msg.get('id') == ("req " + str(kline_req_time)) and msg.get('status') == 'ok' and \
                msg.get('rep') == kline_ch and type(msg.get('data')) == list:
            # print(f"{msg}")
            data = msg.get('data')
            print(f'data len {len(data)}')

            global real_kline_start, kline_start_time, coin_start_time
            if len(data) != 0:
                req_fail_cnt = 0
                pack_len = MAX_KLINE_PER_REQ
                if not real_kline_start:
                    real_kline_start = True
                    pack_len = MAX_KLINE_PER_REQ - int((data[0]['id'] - kline_req_time) / kline_period)
                    kline_req_time = data[0]['id']
                    kline_start_time = kline_req_time
                    if coin_start_time == 0:
                        coin_start_time = kline_start_time
                        state = State.END
                        print(f'check start end coin_start_time {get_time_str(coin_start_time)}')
                        ws.close()
                        switch_next_kline()
                        return

                data = fix_kline_data(data, pack_len)
                save_kline_data(data)
                kline_req_time += len(data) * kline_period
                # print(f'kline_req_time {kline_req_time} REQ_KLINE_END{REQ_KLINE_END}')
                if kline_req_time >= REQ_KLINE_END:
                    state = State.END
                    print('kline req end')
                    ws.close()

                    kline_to_csv()
                    switch_next_kline()
                    return
                else:
                    global req_success_cnt
                    req_success_cnt += 1
                    if req_success_cnt > 20:
                        req_success_cnt = 0
                        on_error(ws, 'req_success_cnt auto')
                        return
            elif not real_kline_start:
                kline_req_time += MAX_KLINE_PER_REQ * kline_period

            time.sleep(0.11)
            send_kline_req(ws)
        else:
            cur_time = time.time()
            if cur_time - kline_send_time >= 5:
                req_fail_cnt += 1
                if req_fail_cnt > 1:
                    req_fail_cnt = 0
                    on_error(ws, 'req_fail_cnt auto')
                else:
                    send_kline_req(ws)


def on_error(ws, error):
    print("on_error")
    print(error)
    ws.close()
    time.sleep(3)
    start_web_socket()


def on_close(ws):
    print("### closed ###")


def on_open(ws):
    print("on_open")
    global state
    state = State.SUB
    ws.send(json.dumps(kline_sub))


def start_web_socket():
    # websocket.enableTrace(True)
    ws = websocket.WebSocketApp("wss://api.huobi.pro/ws",
                                on_open=on_open,
                                on_message=on_message,
                                on_error=on_error,
                                on_close=on_close)

    ws.run_forever(http_proxy_host="127.0.0.1", http_proxy_port=1081)


start_req_kline()
