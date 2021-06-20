import pandas as pd
import mplfinance as mpf
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec


t=[]
for i in range(10):
    t.append(i)
print(t)

t=pd.Series(t)
t=t.rolling(window=3).mean()
print(t)
print(len(t))

