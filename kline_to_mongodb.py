import requests
import time
import asyncio
import websocket
import json
import gzip
from enum import Enum
import pymongo

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"

KLINE_CH = "market.btcusdt.kline.1min"
KLINE_PERIOD = 60

kline_sub = {
    "sub": KLINE_CH,
    "id": "id1"
}


State = Enum('State', ('IDLE', 'SUB', 'REQ', 'END'))
state = State.IDLE
MAX_KLINE_PER_REQ = 720
REQ_KLINE_START = "2017-11-01 00:00:00"
REQ_KLINE_START = int(time.mktime(time.strptime(REQ_KLINE_START, TIME_FORMAT)))
REQ_KLINE_END = "2021-06-13 12:00:00"
REQ_KLINE_END = int(time.mktime(time.strptime(REQ_KLINE_END, TIME_FORMAT)))

kline_req_time = REQ_KLINE_START
kline_send_time = time.time()

huobi_db = pymongo.MongoClient('mongodb://localhost:27017/')['huobi_data']
kline_coll = huobi_db.get_collection(KLINE_CH)
kline_coll.create_index('id')


def check_req_start_time():
    global kline_req_time
    for kline_time in range(REQ_KLINE_START, REQ_KLINE_END, KLINE_PERIOD):
        target_time = {'id': kline_time}
        if kline_coll.count_documents(target_time) == 0:
            kline_req_time = kline_time
            print('kline_req_time %s' % time.strftime(TIME_FORMAT, time.localtime(kline_req_time)))
            return

    print('all kline time exist in db')
    exit()


# check_req_start_time()


def save_kline_data(data):
    if kline_coll.count_documents({'id': data[0]['id']}) == 0:
        kline_coll.insert_many(data)
    else:
        print(f'list @ %d exist' % data[0]['id'])


def send_kline_req(ws):
    global kline_send_time
    to_time = min(REQ_KLINE_END - KLINE_PERIOD, kline_req_time + KLINE_PERIOD * (MAX_KLINE_PER_REQ - 1))
    kline_req = {
        "req": KLINE_CH,
        "id": "req " + str(kline_req_time),
        "from": kline_req_time,
        "to": to_time
    }
    ws.send(json.dumps(kline_req))
    cur_time = time.time()
    req_time_str = time.strftime(TIME_FORMAT, time.localtime(kline_req_time))
    print('req kline %.2f %s %d' % (cur_time - kline_send_time, req_time_str, kline_req_time))
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


def get_kline_item_from_list(id, data):
    global last_kline
    for item in data:
        if (item.get('id') == id and 'open' in item and 'close' in item and 'low' in item and 'high' in item and
                'amount' in item and 'vol' in item and 'count' in item):
            last_kline = item.copy()
            return item

    print("kline lost @ %d %s" % (id, time.strftime(TIME_FORMAT, time.localtime(id))))
    last_kline['id'] = id
    last_kline['open'] = last_kline['close']
    last_kline['low'] = last_kline['close']
    last_kline['high'] = last_kline['close']
    last_kline['amount'] = 0
    last_kline['vol'] = 0
    last_kline['count'] = 0
    return last_kline.copy()


def fix_kline_data(data):
    ret_list = []
    for i in range(MAX_KLINE_PER_REQ):
        ret_list.append(get_kline_item_from_list(kline_req_time + i * KLINE_PERIOD, data))

    return ret_list


def on_message(ws, msg):
    msg = gzip.decompress(msg).decode('utf8')
    msg = json.loads(msg)
    global state, kline_req_time
    # print(f"on_message {msg}")
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

    if state == State.REQ:
        if msg.get('id') == ("req " + str(kline_req_time)) and msg.get('status') == 'ok' and \
                msg.get('rep') == KLINE_CH and type(msg.get('data')) == list:
            # print(f"{msg}")
            data = msg.get('data')
            print(f'data len {len(data)}')
            if len(data) != 0:
                data = fix_kline_data(data)
                save_kline_data(data)
                kline_req_time += len(data) * KLINE_PERIOD
                if kline_req_time >= REQ_KLINE_END:
                    state = State.END
                    print('kline req end')
                    return
            time.sleep(0.11)
            send_kline_req(ws)
        else:
            cur_time = time.time()
            if cur_time - kline_send_time >= 1:
                send_kline_req(ws)


def on_error(ws, error):
    print("on_error")
    print(error)


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


start_web_socket()
