import subprocess
import math

# XXX
# XXX You need to adapt the below values
# XXX also adapt the numMemAccesses list
# XXX

steps = 128
startPow = 1
maxValPow = 19
rerunTimes = 32

#cur = step

print("size,cycles")

points = [ 2**(startPow + x*((maxValPow - startPow)/steps)) for x in range(1,steps) ]

#points = [1,2,4,8,16,32]

for cur in points:
	if rerunTimes > 8:
		rerunTimes = rerunTimes - 2

	for x in range(0,rerunTimes):
		proc = subprocess.run(['./siphash',str(cur)],stdout=subprocess.PIPE)
		print(proc.stdout.decode('utf-8'), end='')
