local ffi = require "ffi"
local memory = require "memory"
local log = require "log"
local utils = require "utils"

ffi.cdef[[
void *HelloByeClient_init();
void *HelloByeClient_config(uint32_t srcIP, uint16_t dstPort);

void *HelloByeClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort);

void HelloByeClient_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void *HelloByeClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void HelloByeClient_free(void *obj);

]]

local mod = {}

function mod.init()
	return ffi.C.HelloByeClient_init()
end

function mod.config(srcIP, dstPort)
	ffi.C.HelloByeClient_config(parseIP4Address(srcIP), dstPort)
end

function mod.process(obj, inPkts, inCount)
	ret = {}

	if 0 < inCount then
		log:info("helloByeClient.process() called (>0 packets)")

		local sendBufsCount = ffi.new("unsigned int[1]")
		local freeBufsCount = ffi.new("unsigned int[1]")

		local ba = ffi.C.HelloByeClient_process(obj, inPkts, inCount, sendBufsCount,
		freeBufsCount)

		local sendBufs = memory.bufArray(sendBufsCount[0])
		local freeBufs = memory.bufArray(freeBufsCount[0])

		ffi.C.HelloByeClient_getPkts(ba, sendBufs.array, freeBufs.array)

		sendBufs.size = sendBufsCount[0]
		ret.send = sendBufs
		ret.sendCount = sendBufsCount[0]

		freeBufs:freeAll()
	else
		ret.sendCount = 0
	end

	return ret
end

function mod.connect(mempool, obj, dstIP, srcPort)

	local bufArray = mempool:bufArray(1)

	bufArray:alloc(100)

	local sendBufsCount = ffi.new("unsigned int[1]")
	local freeBufsCount = ffi.new("unsigned int[1]")

	local bAC = ffi.C.HelloByeClient_connect(obj, bufArray.array, 1, sendBufsCount,
	freeBufsCount, dstIP, srcPort)

	local sendBufs = memory.bufArray(sendBufsCount[0])
	local freeBufs = memory.bufArray(freeBufsCount[0])

	ffi.C.HelloByeClient_getPkts(bAC, sendBufs.array, freeBufs.array)

	sendBufs.size = sendBufsCount[0]

	ret = {}

	ret.send = sendBufs
	ret.sendCount = sendBufsCount[0]

	return ret
end

function mod.free(obj)
	ffi.C.HelloByeClient_free(obj)
end

return mod
