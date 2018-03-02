--- Layer 2 reflector, swaps src and dst MACs and echoes the packet
local lm     = require "libmoon"
local memory = require "memory"
local device = require "device"
local stats  = require "stats"
local lacp   = require "proto.lacp"
local astraeus   = require "astraeusServer"
local arp    = require "proto.arp"
local utils  = require "utils"
local log    = require "log"

-- IP of this host
local RX_IP		= "192.168.0.1"
local dstMacStr		= "3c:fd:fe:9e:d7:40"
local srcMacStr		= "68:05:CA:32:44:D8"

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
				ips = {"192.168.0.1", "192.168.0.2","192.168.0.3","192.168.0.4","192.168.0.5","192.168.0.6","192.168.0.7","192.168.0.8","192.168.0.9","192.168.0.10"} }
		}
	end

	lm.waitForTasks()
end

function reflector(rxQ, txQ)
	local bufs = memory.bufArray()

	local dstMac = parseMacAddress(dstMacStr, true)
	local srcMac = parseMacAddress(srcMacStr, true)

	local state = astraeus.init()

	while lm.running() do
		-- receive some packets
		local rx = rxQ:tryRecv(bufs, 1000)

		-- run packets through the state machine
		local curPkts = astraeus.process(state, bufs.array, rx)

		-- extract packets from output
		local sendBufs = curPkts.send
		local sendBufsCount = curPkts.sendCount

		for i = 1, curPkts.sendCount do
			-- swap MAC addresses
			local pkt = sendBufs[i]:getEthernetPacket()
			--local tmp = pkt.eth:getDst()
			pkt.eth:setDst(dstMac)
			pkt.eth:setSrc(srcMac)
			local vlan = bufs[i]:getVlan()
			if vlan then
				bufs[i]:setVlan(vlan)
			end
		end

		if sendBufsCount > 0 then
			log:info("reflector is sending packets: " .. sendBufsCount)
		end

		if sendBufsCount > 0 then
			sendBufs:offloadIPChecksums(true)
			sendBufs:offloadUdpChecksums(true)

			txQ:sendN(sendBufs, sendBufsCount)
		end
	end

	astraeus.free(state)
end

