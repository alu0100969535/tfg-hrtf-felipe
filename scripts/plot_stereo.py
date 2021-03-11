import matplotlib.pyplot as plt
import csv
import sys
import os 
import re # Reg Exp
from copy import copy


class Data:
	x = []
	y = []

class Samples:
	left = Data()
	right = Data()
	name = ''

##### 

count = 0
type  = 'time'
folder  = ''

if len(sys.argv) < 3:
    print('Please enter mode as 1st parameter: freq or time')
    print('Enter folder as 2nd parameter: C:/data')
    sys.exit()
else:
    type = sys.argv[1]
    folder = sys.argv[2]
    if len(sys.argv) == 4:
        limit = sys.argv[3]

### Create folder results ###

path_folder = os.path.join(folder, 'plots')
try:
    os.mkdir(path_folder)
except FileExistsError:
    print('plots folder is ready')
    
######


findings = []

    
for file in os.listdir(folder):
    if file == 'plots':
        continue

    path = os.path.join(folder, file);
    
    m = re.search('(\w+)\.(left|right)_(\d+)_(time|freq)',file)
    
    name = m.groups()[0] + m.groups()[2]
    speaker = m.groups()[1]
    
    current = None
    
    for item in findings:
        if(item.name == name):
            current = item
    
    if current == None:
        current = Samples()
        current.name = name
        findings.append(current)
    
    x=[]
    y=[]
    # Load file and save into samples obj
    with open(path, 'r') as csvfile:
        plots = csv.reader(csvfile, delimiter=',')
        for row in plots:
            x.append(float(row[1]))
            y.append(float(row[0]))
    
    data = Data()
    data.x = copy(x)
    data.y = copy(y)
            
    if speaker == 'left':
        current.left = data
    else:
        current.right = data

for item in findings:
    
    #plt.figure(figsize=(25,15))
    plt.plot(item.left.x,item.left.y, marker='.')
    plt.plot(item.right.x,item.right.y, marker='.')
    plt.legend(('Canal izquierdo', 'Canal derecho'))
    
    if type == 'freq':
        plt.title('FFT of ' + item.name)
        plt.xlabel('Bins')
        plt.ylabel('Importance')
    elif type == 'time':
        plt.title(item.name)
        plt.xlabel('Muestra')
        plt.ylabel('Amplitud')
        
    path_save = os.path.join(path_folder, item.name)
    plt.savefig(path_save + '.png')
    print('saved ' +  path_save + '.png')
    plt.cla();
    plt.clf();
    plt.close();
    #plt.show()
