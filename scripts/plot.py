import matplotlib.pyplot as plt
import csv
import sys


x=[]
y=[]

count = 0
type  = 'time'
file  = ''
limit = 50000

if len(sys.argv) < 3:
    print('Please enter mode as 1st parameter: freq or time')
    print('Enter filename as 2nd parameter: wave.csv')
    print('Optional: enter a number to plot first values only')
    sys.exit()
else:
    type = sys.argv[1]
    file = sys.argv[2]
    if len(sys.argv) == 4:
        limit = sys.argv[3]

with open(file, 'r') as csvfile:
    plots= csv.reader(csvfile, delimiter=',')
    for row in plots:
        if count < int(limit):
            x.append(float(row[1]))
            y.append(float(row[0]))
        count = count + 1

plt.plot(x,y, marker='.')

if type == 'freq':
    plt.title('FFT of ' + file)
    plt.xlabel('Bins')
    plt.ylabel('Importance')
elif type == 'time':
    plt.title(file)
    plt.xlabel('Time')
    plt.ylabel('Amplitude')


plt.show()
