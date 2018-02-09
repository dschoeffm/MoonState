--- Layer 2 reflector, swaps src and dst MACs and echoes the packet
local lm     = require "libmoon"
local memory = require "memory"
local device = require "device"
local stats  = require "stats"
local lacp   = require "proto.lacp"
local dtls   = require "DtlsClient"
local arp    = require "proto.arp"
local utils  = require "utils"

-- IP of this host
--local RX_IP		= "192.168.0.1"
local dstMacStr		= "3C:FD:FE:9E:D6:B8"
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
			txQueues = args.threads + 2,
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
		lm.startTask("connector", dev:getTxQueue(args.threads))
	end

	for i, dev in ipairs(args.dev) do
		arp.startArpTask{
			-- run ARP on both ports
			{ rxQueue = dev:getRxQueue(args.threads), txQueue = dev:getTxQueue(args.threads+1),
				ips = RX_IP }
		}
	end

	lm.waitForTasks()
end

function connector(txQ)
	-- memory pool with default values for all packets, this is our archetype
	local mempool = memory.createMemPool(function(buf)
		buf:getUdpPacket():fill{
			ethSrc = txQ, -- MAC of the tx device
			ethDst = dstMac
		}
	end)

	-- TODO why doesn't this work?
	--local dstIP = utils.parseIP4Address("192.168.0.2")
	local dstIP = 0xc0a80002
	local srcIP = 0xc1000000

	local state = dtls.init(dstIP, 4433)

	local bSize = 128

	local dstMac = parseMacAddress(dstMacStr, true)
	local srcMac = parseMacAddress(srcMacStr, true)

	-- a bufArray is just a list of buffers from a mempool that is processed as a single batch
	while lm.running() do -- check if Ctrl+c was pressed
		-- this actually allocates some buffers from the mempool the array is associated with
		-- this has to be repeated for each send because sending is asynchronous, we cannot reuse the old buffers here

		local curPkts = dtls.connect(mempool, state, srcIP, 1025, bSize)

		-- this is 64bit large -> should not wrap around
		srcIP = srcIP +1
		if srcIP >= 0xc2000000 then
			srcIP = 0xc1000000
		end

		-- extract packets from output
		local sendBufs = curPkts.send
		local sendBufsCount = curPkts.sendCount

		for i = 1, sendBufsCount do
			local pkt = sendBufs[i]:getEthernetPacket()

			pkt.eth:setDst(dstMac)
			pkt.eth:setSrc(srcMac)
		end

		-- UDP checksums are optional, so using just IPv4 checksums would be sufficient here
		-- UDP checksum offloading is comparatively slow: NICs typically do not support calculating the pseudo-header checksum so this is done in SW
		sendBufs:offloadIPChecksums(true)
		sendBufs:offloadUdpChecksums(true)
		-- send out all packets and frees old bufs that have been sent

		txQ:sendN(sendBufs, sendBufsCount)

		--lm.sleepMicros(10)
	end

	dtls.free(state)
end

function reflector(rxQ, txQ)
	local bufs = memory.bufArray()

	-- setup state machine for dtls
	local state = dtls.init(dstIP, 4433)

	while lm.running() do
		-- receive some packets
		local rx = rxQ:tryRecv(bufs, 1000)

		-- run packets through the state machine
		local curPkts = dtls.process(state, bufs.array, rx)

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

	dtls.free(state)
end

