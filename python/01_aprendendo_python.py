from numpy import cos, sin, pi, arange
from pylab import show, figure, plot, title, legend

T = 20
t = arange(0,100,0.1)
y1 = cos((2*pi*(1/T))*t)
y2 = sin(2*pi*(1/T)*t)
figure(1)
line1,line2 = plot(t,y1,'b',t,y2,'r')
title("Aula 01 de Instrumentação Industrial")
legend([line1,line2], ["COS","SIN"])
show()