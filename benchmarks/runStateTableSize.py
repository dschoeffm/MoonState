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

print("size,numMemAccesses,setup,run")

points = [ 2**(startPow + x*((maxValPow - startPow)/steps)) for x in range(1,steps) ]
numMemAccesses = [ x for x in range(0,128) ]

#print(points)

for cur in points:
	if rerunTimes > 8:
		rerunTimes = rerunTimes - 4

	for n in numMemAccesses:
		curSetup = 999999999999999
		curRun = 999999999999999

		for x in range(0,rerunTimes):
			proc = subprocess.run(['./stateTableSize',str(cur),str(n)],stdout=subprocess.PIPE)
			strings = proc.stdout.decode('utf-8').split('\n')

			curSetup = min(int(strings[0].split(' ')[1]), curSetup)
			curRun = min(int(strings[1].split(' ')[1]), curRun)

		curSetup = (curSetup * 1.0) / cur
		curRun = (curRun * 1.0) / cur
		print(str(cur) + ',' + str(n) + ',' + str(curSetup) + ',' + str(curRun))
#	cur = cur + step
