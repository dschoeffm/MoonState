--- Layer 2 reflector, swaps src and dst MACs and echoes the packet
local lm     = require "libmoon"
local memory = require "memory"
local device = require "device"
local stats  = require "stats"
local lacp   = require "proto.lacp"
local helloBye = require "helloByeServer"
local arp    = require "proto.arp"

-- IP of this host
local RX_IP		= "192.168.0.1"

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
	local state = helloBye.init()

	while lm.running() do
		-- receive some packets
		local rx = rxQ:tryRecv(bufs, 1000)

		-- run packets through the state machine
		local curPkts = helloBye.process(state, bufs.array, rx)

		-- extract packets from output
		local sendBufs = curPkts.send
		local sendBufsCount = curPkts.sendCount

		for i = 1, curPkts.sendCount do
			-- swap MAC addresses
			local pkt = sendBufs[i]:getEthernetPacket()
			local tmp = pkt.eth:getDst()
			pkt.eth:setDst(pkt.eth:getSrc())
			pkt.eth:setSrc(tmp)
			local vlan = bufs[i]:getVlan()
			if vlan then
				bufs[i]:setVlan(vlan)
			end
		end

		if sendBufsCount > 0 then
			sendBufs:offloadIPChecksums(true, 14,20,1)
			sendBufs:offloadUdpChecksums(true)

			txQ:sendN(sendBufs, sendBufsCount)
		end
	end
end

