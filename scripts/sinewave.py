import numpy as np
from scipy.io import wavfile

freq =np.array([440,493,523,587,659,698,783,880,1500,3500,5000,7000,10000]) #tone frequencies

fs=44100    #sample rate
duration=5  #signal duration

t=np.arange(0,duration,1./fs) #time

for i in range(0,len(freq)):
    music = []
    x = 10000 * np.cos(2 * np.pi * freq[i] * t) #generated signals
    music = np.hstack((music, x))

    wavfile.write(str(freq[i]) + '.wav', fs, music.astype(np.dtype('i2')))