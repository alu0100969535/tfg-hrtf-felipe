# Import libraries
from mpl_toolkits import mplot3d
import numpy as np
import matplotlib.pyplot as plt
import math

def rad(x):
    rad=x*(math.pi/180)
    return rad
    
distance = 1.4
 
elevation = [-40, -30, -20, -10, 0, 10, 20, 30, 40, 50, 60, 70, 80, 90]
measurements = [56, 60, 72, 72, 72, 72, 72, 60, 56, 45, 36, 24, 12, 1]
azimuth_increment = [6.43, 6.00, 5.00, 5.00, 5.00, 5.00, 5.00, 6.00, 6.43, 8.00, 10.00, 15.00, 30.00, 0]

z = []
x = []
y = []

# Creating figure
fig = plt.figure(figsize = (10, 7))
ax = plt.axes(projection ="3d")
plt.title("HRTF filters coverage")  
 
# Creating dataset
index = 0
for elev in elevation:
    for i in range(0, measurements[index]):
        
        e = rad(elev)
        azimuth = rad(i * azimuth_increment[index])
        
        x.append(distance * math.cos(azimuth) * math.cos(e))
        y.append(distance * math.sin(azimuth) * math.cos(e))
        z.append(distance * math.sin(e))
        
    ax.scatter3D(x, y, z, label=elev)
    index = index + 1
    # Reset data
    x = []
    y = []
    z = []
    
    
# Creating plot

ax.legend(title = "Elevation")
# show plot
plt.show()