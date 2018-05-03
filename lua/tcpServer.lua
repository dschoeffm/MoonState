local ffi = require "ffi"
local memory = require "memory"
local log = require "log"

ffi.cdef[[
void *TCP_Server_Joke_init(struct mempool *mp);

void *TCP_Server_Joke_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

void TCP_Server_Joke_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

void TCP_Server_Joke_free(void *obj);
]]

local mod = {}

function mod.init()
	ret = {}
	ret.mempool = memory.createMemPool()
	ret.obj = ffi.C.TCP_Server_Joke_init(ret.mempool)
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

		local ba = ffi.C.TCP_Server_Joke_process(obj.obj, inPkts, inCount, obj.sbc,
		obj.fbc)

		if obj.sbc[0] > obj.sbufsS then
			obj.sbufs = memory.bufArray(obj.sbc[0])
			obj.sbufsS = obj.sbc[0]
		end

		if obj.fbc[0] > obj.fbufsS then
			obj.fbufs = memory.bufArray(obj.fbc[0])
			obj.fbufsS = obj.fbc[0]
		end

		ffi.C.TCP_Server_Joke_getPkts(ba, obj.sbufs.array, obj.fbufs.array)

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
	ffi.C.TCP_Server_Joke_free(obj.obj)
end

return mod
