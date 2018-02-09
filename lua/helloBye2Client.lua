local ffi = require "ffi"
local memory = require "memory"
local log = require "log"
local utils = require "utils"

ffi.cdef[[
void *HelloBye2_Client_init();

void HelloBye2_Client_config(uint32_t srcIP, uint16_t dstPort);

void *HelloBye2_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort,
	uint64_t ident);

void HelloBye2_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void *HelloBye2_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void HelloBye2_Client_free(void *obj);
]]

local mod = {}

function mod.init()
	ret = {}
	ret.obj = ffi.C.HelloBye2_Client_init()
	ret.sbc = ffi.new("unsigned int[1]")
	ret.fbc = ffi.new("unsigned int[1]")
	ret.fbufs = memory.bufArray(128)
	ret.fbufsS = 128
	ret.sbufs = memory.bufArray(128)
	ret.sbufsS = 128
	return ret
end

function mod.config(srcIP, dstPort)
	ffi.C.HelloBye2_Client_config(parseIP4Address(srcIP), dstPort)
end

function mod.process(obj, inPkts, inCount)
	ret = {}

	if 0 < inCount then
--		log:info("helloBye.process() called (>0 packets)")

		local ba = ffi.C.HelloBye2_Client_process(obj.obj, inPkts, inCount, obj.sbc,
		obj.fbc)

		if obj.sbc[0] > obj.sbufsS then
			obj.sbufs = memory.bufArray(obj.sbc[0])
			obj.sbufsS = obj.sbc[0]
		end

		if obj.fbc[0] > obj.fbufsS then
			obj.fbufs = memory.bufArray(obj.fbc[0])
			obj.fbufsS = obj.fbc[0]
		end

		ffi.C.HelloBye2_Client_getPkts(ba, obj.sbufs.array, obj.fbufs.array)

		obj.sbufs.size = obj.sbc[0]
		ret.send = obj.sbufs
		ret.sendCount = obj.sbc[0]

		obj.fbufs:freeAll()
	else
		ret.sendCount = 0
	end

	return ret
end

function mod.connect(mempool, obj, dstIP, srcPort, ident, bSize)
	local bufArray = mempool:bufArray(bSize)
	bufArray:alloc(100)

	local sendBufsAll = memory.bufArray(bSize)

	for i = 1,bSize do

		local bAC = ffi.C.HelloBye2_Client_connect(obj.obj, bufArray.array + (i-1), 1, obj.sbc,
		obj.fbc, dstIP, srcPort, ident +( i-1))

		local freeBufs = memory.bufArray(obj.fbc[0])

		ffi.C.HelloBye2_Client_getPkts(bAC, sendBufsAll.array + (i-1), freeBufs.array)

	end

	sendBufsAll.size = bSize

	ret = {}

	ret.send = sendBufsAll
	ret.sendCount = bSize

	return ret
end

function mod.free(obj)
	ffi.C.HelloBye2_Client_free(obj.obj)
end

return mod
