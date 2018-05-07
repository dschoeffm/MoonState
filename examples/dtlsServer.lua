--- Layer 2 reflector, swaps src and dst MACs and echoes the packet
local lm     = require "libmoon"
local memory = require "memory"
local device = require "device"
local stats  = require "stats"
local lacp   = require "proto.lacp"
local dtls   = require "dtlsServer"
local arp    = require "proto.arp"
local log = require "log"
local ffi = require "ffi"

-- IP of this host
local RX_IP		= "192.168.0.2"

function configure(parser)
	parser:argument("dev", "Devices to use."):args("+"):convert(tonumber)
	parser:option("-t --threads", "Number of threads per device."):args(1):convert(tonumber):default(1)
	return parser:parse()
end

function master(args)
	local lacpQueues = {}
	for i, dev in ipairs(args.dev) do
		local dev = device.config{
			port = dev,
			rxQueues = args.threads + 1,
			txQueues = args.threads + 1,
			rssQueues = args.threads
		}
		args.dev[i] = dev
	end
	device.waitForLinks()

	-- print statistics
	stats.startStatsTask{devices = args.dev}

	for i, dev in ipairs(args.dev) do
		for i = 1, args.threads do
			lm.startTask("reflector", dev:getRxQueue(i-1), dev:getTxQueue(i-1))
		end
	end

	for i, dev in ipairs(args.dev) do
		arp.startArpTask{
			-- run ARP on both ports
			{ rxQueue = dev:getRxQueue(args.threads), txQueue = dev:getTxQueue(args.threads),
				ips = RX_IP }
		}
	end

	lm.waitForTasks()
end

function reflector(rxQ, txQ)
	local bufs = memory.bufArray()

	-- setup state machine for the hello bye protocol
	local state = dtls.init()

	while lm.running() do
		-- receive some packets
		local rx = rxQ:tryRecv(bufs, 1000)

		-- run packets through the state machine
		dtls.process(state, bufs.array, rx)

		if state.sbc[0] > 0 then
			state.sbufs:offloadIPChecksums(true, 14,20,1)
			state.sbufs:offloadUdpChecksums(true)

			txQ:sendN(state.sbufs, state.sbc[0])
		end
	end

	dtls.free(state)
end

