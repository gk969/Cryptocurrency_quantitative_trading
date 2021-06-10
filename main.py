import requests
import time
import asyncio
import websocket
import json
import gzip
from enum import Enum

kline_sub = {
    "sub": "market.ethusdt.kline.1min",
    "id": "id1"
}

State = Enum('State', ('IDLE', 'SUB', 'REQ'))
state = State.IDLE


def on_message(ws, msg):
    msg = gzip.decompress(msg).decode('utf8')
    msg = json.loads(msg)
    global state
    print(f"on_message {msg}")
    if "ping" in msg:
        pong = {"pong": msg['ping']}
        ws.send(json.dumps(pong))

    if state == State.SUB:
        if 'id' in msg and 'subbed' in msg and 'status' in msg:
            if msg['id'] == kline_sub['id'] and msg['subbed'] == kline_sub['sub'] and msg['status'] == 'ok':
                state = State.REQ

                return
            else:
                print(f"sub kline error : {msg}")
        ws.send(json.dumps(kline_sub))


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


# websocket.enableTrace(True)
ws = websocket.WebSocketApp("wss://api.huobi.pro/ws",
                            on_open=on_open,
                            on_message=on_message,
                            on_error=on_error,
                            on_close=on_close)

ws.run_forever(http_proxy_host="127.0.0.1", http_proxy_port=1081)

# from_time = "2017-11-00 00:00:00"
# kline_req = requests.get('https://api.hbdm.com/linear-swap-ex/market/history/kline',
#                          params={'q': 'python', 'cat': '1001'}, timeout=5)
