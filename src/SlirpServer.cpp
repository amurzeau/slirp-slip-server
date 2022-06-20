// SPDX-License-Identifier: MIT

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "SlirpServer.h"
#include <algorithm>
#include <libslirp.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#include <uv.h>

const uint8_t SlirpServer::SLIRP_ETHER_HEADER[SLIRP_ETHER_HEADER_SIZE] = {
    /* dst */
    0x52,
    0x55,
    0x0a,
    0x00,
    0x02,
    0x01,
    /* src */
    0x52,
    0x55,
    0x0a,
    0x00,
    0x02,
    0x0e,
    /* Type: IPv4 */
    0x08,
    0x00,
};

SlirpServer::SlirpServer() {}

void SlirpServer::init(bool disableHostAccess, const std::vector<std::pair<uint16_t, uint16_t>>& forwardedPorts) {
	SlirpConfig config = {
	    .version = 4,
	    .restricted = false,
	    .in_enabled = true,
	    .vnetwork = {.S_un = {.S_addr = inet_addr("192.168.10.0")}},
	    .vnetmask = {.S_un = {.S_addr = inet_addr("255.255.255.0")}},
	    .vhost = {.S_un = {.S_addr = inet_addr("192.168.10.1")}},
	    .vdhcp_start = {.S_un = {.S_addr = inet_addr("192.168.10.15")}},
	    .vnameserver = {.S_un = {.S_addr = inet_addr("192.168.10.2")}},
	    .disable_host_loopback = disableHostAccess,
	    .enable_emu = false,
	    .disable_dns = false,
	    .disable_dhcp = false,
	};

	static struct SlirpCb callbacks = {
	    .send_packet = &SlirpServer::onSlirpWrite,
	    .guest_error = &SlirpServer::onSlirpGuestError,
	    .clock_get_ns = &SlirpServer::onSlirpClockGetNs,
	    .timer_free = &SlirpServer::onSlirpTimerFree,
	    .timer_mod = &SlirpServer::onSlirpTimerMod,
	    .register_poll_fd = &SlirpServer::onSlirpRegisterFd,
	    .unregister_poll_fd = &SlirpServer::onSlirpUnregisterFd,
	    .notify = &SlirpServer::onSlirpNotify,
	    .timer_new_opaque = &SlirpServer::onSlirpTimerNew,
	};

	SPDLOG_INFO("Gateway IP: 192.168.10.1");
	SPDLOG_INFO("DNS gateway: 192.168.10.2");
	SPDLOG_INFO("Guest IP: 192.168.10.15");
	SPDLOG_INFO("Virtual network: 192.168.10.0/24");

	if(disableHostAccess)
		SPDLOG_INFO("Access to host ports is disabled");

	slirpHandle = slirp_new(&config, &callbacks, this);

	struct in_addr localhost = {.S_un = {.S_addr = inet_addr("127.0.0.1")}};
	struct in_addr guestAddr = {.S_un = {.S_addr = inet_addr("192.168.10.15")}};

	for(const auto& portToForward : forwardedPorts) {
		SPDLOG_INFO("Forwarded port: 127.0.0.1:{} -> 192.168.10.15:{}", portToForward.first, portToForward.second);

		if(slirp_add_hostfwd(slirpHandle, 0, localhost, portToForward.first, guestAddr, portToForward.second) != 0) {
			SPDLOG_ERROR("Failed to forward host port {} to guest port {}", portToForward.first, portToForward.second);
		}
	}

	uv_prepare_init(uv_default_loop(), &prepareHandle);
	prepareHandle.data = this;
	uv_prepare_start(&prepareHandle, &SlirpServer::onSlirpPrepareStatic);

	uv_timer_init(uv_default_loop(), &pollTimerHandle);
	pollTimerHandle.data = this;

	updateArpTable();
}

void SlirpServer::attachClient(ISlirpClient* client) {
	if(this->slirpClient) {
		this->slirpClient->close();
	}
	this->slirpClient = client;
}

void SlirpServer::detachClient(ISlirpClient* client) {
	if(this->slirpClient == client)
		this->slirpClient = nullptr;
}

void SlirpServer::updateArpTable() {
	const uint8_t* sourceMac = &SLIRP_ETHER_HEADER[6];
	const uint8_t* destinationMac = &SLIRP_ETHER_HEADER[0];
	const uint8_t sourceIp[] = {192, 168, 10, 15};
	const uint8_t destinationIp[] = {192, 168, 10, 1};

	SPDLOG_DEBUG("Sending ARP packet to SLIRP");

	std::vector<uint8_t> arpPacket;
	arpPacket.reserve(28);
	arpPacket.assign(SLIRP_ETHER_HEADER, SLIRP_ETHER_HEADER + SLIRP_ETHER_HEADER_SIZE);

	// EtherType = ARP
	arpPacket[12] = 0x08;
	arpPacket[13] = 0x06;

	// HardwareType = Ethernet
	arpPacket.push_back(0x00);
	arpPacket.push_back(0x01);

	// ProtocolType = IPv4
	arpPacket.push_back(0x08);
	arpPacket.push_back(0x00);

	// Hardware Address Length
	arpPacket.push_back(0x06);
	// Protocol Address Length
	arpPacket.push_back(0x04);

	// Operation = Reply
	arpPacket.push_back(0x00);
	arpPacket.push_back(0x02);

	// Source MAC
	std::copy_n(sourceMac, 6, std::back_inserter(arpPacket));

	// Source IP
	std::copy_n(sourceIp, 4, std::back_inserter(arpPacket));

	// Destination MAC
	std::copy_n(destinationMac, 6, std::back_inserter(arpPacket));

	// Destination IP
	std::copy_n(destinationIp, 4, std::back_inserter(arpPacket));

	slirp_input(slirpHandle, &arpPacket[0], (int) arpPacket.size());
}

void SlirpServer::updateSlirpPollFds() {
	if(updateSlirpPoll) {
		updateSlirpPoll = false;

		slirp_pollfds_poll(slirpHandle, 0, &SlirpServer::getSlirpRevents, this);

		uint32_t timeout = UINT32_MAX;
		activeFds.clear();
		std::for_each(fdsToPoll.begin(), fdsToPoll.end(), [this](const auto& pair) { activeFds.insert(pair.first); });
		slirp_pollfds_fill(slirpHandle, &timeout, &SlirpServer::addSlirpFdToPoll, this);

		for(auto& removedFd : activeFds) {
			auto it = fdsToPoll.find(removedFd);
			if(it != fdsToPoll.end()) {
				SPDLOG_DEBUG("removing poll on fd {}", removedFd);
				uv_poll_stop(&it->second->pollHandle);
				uv_close((uv_handle_t*) &it->second->pollHandle, &SlirpServer::onSlirpPollClose);
				it->second.release();  // will be freed by the onClosePoll function
				fdsToPoll.erase(it);
			} else {
				SPDLOG_WARN("removed fd not found in fdsToPoll: {}", removedFd);
			}
		}

		if(timeout != UINT32_MAX) {
			uv_timer_start(&pollTimerHandle, &SlirpServer::onSlirpPollTimeout, timeout, 0);
		} else {
			uv_timer_stop(&pollTimerHandle);
		}
	}
}

int SlirpServer::addSlirpFdToPoll(int fd, int events, void* opaque) {
	SlirpServer* thisInstance = (SlirpServer*) opaque;
	FdInfo* fdInfo;

	if(thisInstance->fdsToPoll.contains(fd)) {
		fdInfo = thisInstance->fdsToPoll[fd].get();
	} else {
		SPDLOG_DEBUG("new poll fd {}", fd);
		thisInstance->fdsToPoll[fd] = std::make_unique<FdInfo>();
		fdInfo = thisInstance->fdsToPoll[fd].get();
		uv_poll_init_socket(uv_default_loop(), &fdInfo->pollHandle, fd);
		fdInfo->fd = fd;
		fdInfo->events = 0;
		fdInfo->pollHandle.data = fdInfo;
		fdInfo->connection = thisInstance;
		fdInfo->connection = thisInstance;
		fdInfo->activeUvEvents = 0;
	}

	thisInstance->activeFds.erase(fd);

	int uvEvents = 0;

	if(events & SLIRP_POLL_IN)
		uvEvents |= UV_READABLE;
	if(events & SLIRP_POLL_OUT)
		uvEvents |= UV_WRITABLE;
	//	if(events & SLIRP_POLL_HUP)
	//		uvEvents |= UV_DISCONNECT;

	if(fdInfo->activeUvEvents == uvEvents) {
		SPDLOG_DEBUG("already polling on fd {} with uv events {}", fd, uvEvents);
		return fd;
	}

	fdInfo->activeUvEvents = uvEvents;

	SPDLOG_DEBUG("start poll on fd {} with uv events {}", fd, uvEvents);
	uv_poll_start(&fdInfo->pollHandle, uvEvents, &SlirpServer::onSlirpPoll);

	return fd;
}

int SlirpServer::getSlirpRevents(int fd, void* opaque) {
	SlirpServer* thisInstance = (SlirpServer*) opaque;

	auto it = thisInstance->fdsToPoll.find(fd);
	if(it != thisInstance->fdsToPoll.end()) {
		if(it->second->events) {
			SPDLOG_DEBUG("poll revents on fd {} = {}", fd, it->second->events);
		}
		int events = it->second->events;
		it->second->events = 0;
		return events;
	} else {
		SPDLOG_DEBUG("poll get revents not found fd {}", fd);
		return 0;
	}
}

void SlirpServer::onSlirpPrepare(uv_prepare_t* handle) {
	(void) handle;
	updateSlirpPollFds();
}

void SlirpServer::onSlirpPoll(uv_poll_t* handle, int status, int events) {
	FdInfo* thisInstance = (FdInfo*) handle->data;
	thisInstance->connection->updateSlirpPoll = true;

	if(status < 0) {
		SPDLOG_ERROR("poll failed: {}", -status);
	} else {
		SPDLOG_DEBUG("poll triggerred on {} with uv events {}", thisInstance->fd, events);

		if(events & UV_READABLE)
			thisInstance->events |= SLIRP_POLL_IN;
		if(events & UV_WRITABLE)
			thisInstance->events |= SLIRP_POLL_OUT;
		if(events & UV_DISCONNECT)
			thisInstance->events |= SLIRP_POLL_HUP;
	}

	thisInstance->activeUvEvents = 0;
	uv_poll_stop(handle);
}

void SlirpServer::onSlirpPollClose(uv_handle_t* handle) {
	FdInfo* thisInstance = (FdInfo*) handle->data;
	SPDLOG_DEBUG("freeing poll handle for fd {}", thisInstance->fd);
	delete thisInstance;
}

void SlirpServer::onSlirpPollTimeout(uv_timer_t* timer) {
	SlirpServer* thisInstance = (SlirpServer*) timer->data;
	thisInstance->updateSlirpPoll = true;
	SPDLOG_DEBUG("poll timeout");
}

void SlirpServer::receivePacketFromGuest(const void* data, size_t len) {
	SPDLOG_DEBUG("Received SLIP packet: {:a}", spdlog::to_hex((uint8_t*) data, (uint8_t*) data + len, 16));

	slirp_input(slirpHandle, (const uint8_t*) data, (int) len);
	updateSlirpPoll = true;
}

slirp_ssize_t SlirpServer::onSlirpWrite(const void* buf, size_t len, void* opaque) {
	SlirpServer* thisInstance = (SlirpServer*) opaque;
	const uint8_t* bufToSend = ((const uint8_t*) buf);

	SPDLOG_DEBUG("Sending {} bytes from pipe: {:a}", len, spdlog::to_hex(bufToSend, bufToSend + len, 16));

	thisInstance->slirpClient->sendSlirpPacketToGuest(bufToSend, len);

	return (slirp_ssize_t) len;
}

void SlirpServer::onSlirpGuestError(const char* msg, void* opaque) {
	(void) opaque;
	SPDLOG_ERROR("{}", msg);
}

int64_t SlirpServer::onSlirpClockGetNs(void* opaque) {
	(void) opaque;
	return (int64_t) uv_hrtime();
}

void* SlirpServer::onSlirpTimerNew(SlirpTimerId id, void* cb_opaque, void* opaque) {
	SlirpTimer* timer = new SlirpTimer;
	timer->id = id;
	timer->cb_opaque = cb_opaque;
	timer->pipeConnection = (SlirpServer*) opaque;
	timer->timerHandle.data = timer;
	uv_timer_init(uv_default_loop(), &timer->timerHandle);

	return timer;
}

void SlirpServer::onSlirpTimerFree(void* timer, void* opaque) {
	(void) opaque;

	delete((SlirpTimer*) timer);
}

void SlirpServer::onSlirpTimerMod(void* timer, int64_t expire_time, void* opaque) {
	(void) opaque;

	SlirpTimer* slirpTimer = (SlirpTimer*) timer;
	uv_timer_start(&slirpTimer->timerHandle, &SlirpServer::onSlirpTimerExpired, expire_time, 0);
}

void SlirpServer::onSlirpTimerExpired(uv_timer_t* timer) {
	SlirpTimer* slirpTimer = (SlirpTimer*) timer->data;
	slirp_handle_timer(slirpTimer->pipeConnection->slirpHandle, slirpTimer->id, slirpTimer->cb_opaque);
}

void SlirpServer::onSlirpRegisterFd(int fd, void* opaque) {
	(void) fd;
	(void) opaque;
}

void SlirpServer::onSlirpUnregisterFd(int fd, void* opaque) {
	(void) fd;
	(void) opaque;
}

void SlirpServer::onSlirpNotify(void* opaque) {
	SlirpServer* thisInstance = (SlirpServer*) opaque;
	SPDLOG_DEBUG("poll notify");
	thisInstance->updateSlirpPoll = true;
}
