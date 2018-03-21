import subprocess

step = 128
maxVal = 8192
rerunTimes = 100

cur = step

print("size,setup,run")

while cur < maxVal:
	curSetup = 999999999999999
	curRun = 999999999999999

	for x in range(0,rerunTimes):
		proc = subprocess.run(['./stateTableSize',str(cur)],stdout=subprocess.PIPE)
		strings = proc.stdout.decode('utf-8').split('\n')

		curSetup = min(int(strings[0].split(' ')[1]), curSetup)
		curRun = min(int(strings[1].split(' ')[1]), curRun)

	curSetup = (curSetup * 1.0) / cur
	curRun = (curRun * 1.0) / cur
	print(str(cur) + ',' + str(curSetup) + ',' + str(curRun))
	cur = cur + step
