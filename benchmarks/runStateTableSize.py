import subprocess
import math

# XXX
# XXX You need to adapt the below values
# XXX also adapt the numMemAccesses list
# XXX

steps = 128
startPow = 10
maxValPow = 24
rerunTimes = 128

#cur = step

print("size,rerun,setup,run")

points = [ 2**(startPow + x*((maxValPow - startPow)/steps)) for x in range(1,steps) ]
numMemAccesses = [ x for x in range(0,1) ]

#print(points)

for cur in points:
	if rerunTimes > 8:
		rerunTimes = rerunTimes - 4

	for n in numMemAccesses:

		for x in range(0,rerunTimes):
			proc = subprocess.run(['./stateTableSize',str(cur),str(n)],stdout=subprocess.PIPE)
			strings = proc.stdout.decode('utf-8').split('\n')

			curSetup = int(strings[0].split(' ')[1])
			curRun = int(strings[1].split(' ')[1])

			curSetup = (curSetup * 1.0) / cur
			curRun = (curRun * 1.0) / cur
			print(str(cur) + ',' + str(x) + ',' + str(curSetup) + ',' + str(curRun))
