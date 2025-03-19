import numpy as np
import pandas as pd
from matplotlib import pyplot as plt

N = 16384
t = np.linspace(0,1,N)*2*np.pi

x = 1/25 * np.cos(1/25 *t) + 1/4 * np.sin(7*t)
y = 1/13 * np.sin(5*t - 2) + 1/4 * np.cos(1/10*t + 5)

plt.plot(t,x,t,y)
plt.title("Custom Waveforms")
plt.show()

pd.DataFrame(x).to_csv('arb_waveform3.csv', index=False, header=False, float_format=np.float64)
pd.DataFrame(y).to_csv('arb_waveform4.csv', index=False, header=False, float_format=np.float64)


