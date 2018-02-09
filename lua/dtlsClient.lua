local ffi = require "ffi"
local memory = require "memory"
local log = require "log"
local utils = require "utils"

ffi.cdef[[
void *DtlsClient_init(uint32_t dstIP, uint16_t dstPort);

void *DtlsClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint16_t srcPort);

void DtlsClient_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void *DtlsClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void DtlsClient_free(void *obj);
]]

local mod = {}

function mod.init(dstIP, dstPort)
	ret = {}
	ret.obj = ffi.C.DtlsClient_init(dstIP, dstPort)
	ret.sbc = ffi.new("unsigned int[1]")
	ret.fbc = ffi.new("unsigned int[1]")
	ret.fbufs = memory.bufArray(128)
	ret.fbufsS = 128
	ret.sbufs = memory.bufArray(128)
	ret.sbufsS = 128
	return ret
end

function mod.process(obj, inPkts, inCount)
	ret = {}

	if 0 < inCount then
--		log:info("helloBye.process() called (>0 packets)")

		local ba = ffi.C.DtlsClient_process(obj.obj, inPkts, inCount, obj.sbc,
		obj.fbc)

		if obj.sbc[0] > obj.sbufsS then
			obj.sbufs = memory.bufArray(obj.sbc[0])
			obj.sbufsS = obj.sbc[0]
		end

		if obj.fbc[0] > obj.fbufsS then
			obj.fbufs = memory.bufArray(obj.fbc[0])
			obj.fbufsS = obj.fbc[0]
		end

		ffi.C.DtlsClient_getPkts(ba, obj.sbufs.array, obj.fbufs.array)

		obj.sbufs.size = obj.sbc[0]
		ret.send = obj.sbufs
		ret.sendCount = obj.sbc[0]

		obj.fbufs:freeAll()
	else
		ret.sendCount = 0
	end

	return ret
end

function mod.connect(mempool, obj, srcIP, srcPort, bSize)
	local bufArray = mempool:bufArray(bSize)
	bufArray:alloc(100)

	local sendBufsAll = memory.bufArray(bSize)

	for i = 1,bSize do

		local bAC = ffi.C.DtlsClient_connect(obj.obj, bufArray.array + (i-1), 1, obj.sbc,
		obj.fbc, srcIP, srcPort+i)

		local freeBufs = memory.bufArray(obj.fbc[0])

		ffi.C.DtlsClient_getPkts(bAC, sendBufsAll.array + (i-1), freeBufs.array)

	end

	sendBufsAll.size = bSize

	ret = {}

	ret.send = sendBufsAll
	ret.sendCount = bSize

	return ret
end

function mod.free(obj)
	ffi.C.DtlsClient_free(obj.obj)
end

return mod
