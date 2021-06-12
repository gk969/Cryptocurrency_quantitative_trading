import requests
import time
import asyncio
import websocket
import json
import gzip
from enum import Enum

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"

KLINE_CH = "market.ethusdt.kline.1min"
KLINE_PERIOD = 60

kline_sub = {
    "sub": KLINE_CH,
    "id": "id1"
}

State = Enum('State', ('IDLE', 'SUB', 'REQ', 'END'))
state = State.IDLE
MAX_KLINE_PER_REQ = 720
# REQ_KLINE_START = "2017-11-01 00:00:00"
REQ_KLINE_START = "2021-06-01 12:00:00"
REQ_KLINE_START = int(time.mktime(time.strptime(REQ_KLINE_START, TIME_FORMAT)))
REQ_KLINE_END = "2021-06-12 23:00:00"
REQ_KLINE_END = int(time.mktime(time.strptime(REQ_KLINE_END, TIME_FORMAT)))

kline_req_time = REQ_KLINE_START
kline_send_time = time.time()


def send_kline_req(ws):
    global kline_send_time
    to_time = min(REQ_KLINE_END, kline_req_time + KLINE_PERIOD * (MAX_KLINE_PER_REQ - 1))
    kline_req = {
        "req": KLINE_CH,
        "id": "req " + str(kline_req_time),
        "from": kline_req_time,
        "to": to_time
    }
    print('req kline %s' % time.strftime(TIME_FORMAT, time.localtime(kline_req_time)))
    ws.send(json.dumps(kline_req))
    kline_send_time = time.time()


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


def is_kline_data_valid(data):
    global kline_req_time
    if data[0].get('id') != kline_req_time:
        print('kline start error')
        return False

    for i in range(len(data)):
        item = data[i]
        if not ('id' in item and 'open' in item and 'close' in item and 'low' in item and 'high' in item and
                'amount' in item and 'vol' in item and 'count' in item):
            print('kline key error')
            return False

        if item.get('id') != (kline_req_time + i * KLINE_PERIOD):
            print('kline id error')
            return False

    return True


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
            if is_kline_data_valid(data):
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
