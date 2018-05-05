local ffi = require "ffi"
local memory = require "memory"
local log = require "log"

ffi.cdef[[
void *DtlsServer_init(struct mempool*);

void DtlsServer_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void *DtlsServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void DtlsServer_free(void *obj);
]]

local mod = {}

function mod.init()

	ret = {}
	ret.mempool = memory.createMemPool()
	ret.obj = ffi.C.DtlsServer_init(ret.mempool)

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

	obj.sbc[0] = 0
	obj.fbc[0] = 0

	if 0 < inCount then

		local ba = ffi.C.DtlsServer_process(obj.obj, inPkts, inCount, obj.sbc,
		obj.fbc)

		if obj.sbc[0] > obj.sbufsS then
			obj.sbufs = memory.bufArray(obj.sbc[0])
			obj.sbufsS = obj.sbc[0]
		end

		if obj.fbc[0] > obj.fbufsS then
			obj.fbufs = memory.bufArray(obj.fbc[0])
			obj.fbufsS = obj.fbc[0]
		end

		ffi.C.DtlsServer_getPkts(ba, obj.sbufs.array, obj.fbufs.array)

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
	ffi.C.DtlsServer_free(obj.obj)
end

return mod
