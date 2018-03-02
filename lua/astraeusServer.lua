local ffi = require "ffi"
local memory = require "memory"
local log = require "log"
local utils = require "utils"

ffi.cdef[[
void *AstraeusServer_init();

void AstraeusServer_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void *AstraeusServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void AstraeusServer_free(void *obj);
]]

local mod = {}

function mod.init()
	ret = {}
	ret.obj = ffi.C.AstraeusClient_init()
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
		log:info("astraeusServer.process() called (>0 packets)")

		local ba = ffi.C.AstraeusServer_process(obj.obj, inPkts, inCount, obj.sbc,
		obj.fbc)

		if obj.sbc[0] > obj.sbufsS then
			obj.sbufs = memory.bufArray(obj.sbc[0])
			obj.sbufsS = obj.sbc[0]
		end

		if obj.fbc[0] > obj.fbufsS then
			obj.fbufs = memory.bufArray(obj.fbc[0])
			obj.fbufsS = obj.fbc[0]
		end

		ffi.C.AstraeusServer_getPkts(ba, obj.sbufs.array, obj.fbufs.array)

		obj.sbufs.size = obj.sbc[0]
		ret.send = obj.sbufs
		ret.sendCount = obj.sbc[0]

		obj.fbufs:freeAll()
	else
		ret.sendCount = 0
	end

	return ret
end

function mod.free(obj)
	ffi.C.AstraeusServer_free(obj.obj)
end

return mod
