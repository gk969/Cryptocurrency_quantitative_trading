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
import subprocess

import utils

KLINE_CH = "market.ethusdt.kline.1min"
KLINE_PERIOD = 60

KLINE_START_TIME = "2017-11-01 00:00:00"
KLINE_START_TIME = utils.get_time_num(KLINE_START_TIME)
print(f'KLINE_START_TIME {KLINE_START_TIME}')
TRAINING_START_TIME = "2021-06-08 00:00:00"
TRAINING_START = int((utils.get_time_num(TRAINING_START_TIME)-KLINE_START_TIME)/KLINE_PERIOD)
TRAINING_END_TIME = "2021-06-10 00:00:00"
TRAINING_END = int((utils.get_time_num(TRAINING_END_TIME)-KLINE_START_TIME)/KLINE_PERIOD)

cmd=f'strategy_training\\x64\\Debug\\strategy_training.exe {KLINE_CH} {TRAINING_START} {TRAINING_END}'
print(cmd)

# ret = os.system(cmd)
# print(f'ret {ret}')
