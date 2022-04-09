import time

TIME_FORMAT = "%Y-%m-%d %H:%M:%S"


def get_time_str(time_num):
    return time.strftime(TIME_FORMAT, time.localtime(time_num))


def get_time_num(time_str):
    return int(time.mktime(time.strptime(time_str, TIME_FORMAT)))
