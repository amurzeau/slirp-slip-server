// SPDX-License-Identifier: MIT

#pragma once

#include "ISlirpClient.h"
#include <functional>
#include <libslirp.h>
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <uv.h>
#include <vector>

class SlirpServer {
public:
	SlirpServer();

	void init(bool disableHostAccess, const std::vector<std::pair<uint16_t, uint16_t>>& forwardedPorts);
	void attachClient(ISlirpClient* client);
	void detachClient(ISlirpClient* client);

	void receivePacketFromGuest(const void* data, size_t len);

	constexpr static uint8_t SLIRP_ETHER_HEADER_SIZE = 14;
	const static uint8_t SLIRP_ETHER_HEADER[SLIRP_ETHER_HEADER_SIZE];

private:
	// functions
	void resetInputBuffer();
	void updateArpTable();
	void updateSlirpPollFds();
	static int addSlirpFdToPoll(int fd, int events, void* opaque);
	static int getSlirpRevents(int idx, void* opaque);

private:
	// callbacks
	static void onSlirpPrepareStatic(uv_prepare_t* handle) { ((SlirpServer*) handle->data)->onSlirpPrepare(handle); }
	void onSlirpPrepare(uv_prepare_t* handle);

	static void onSlirpPoll(uv_poll_t* handle, int status, int events);
	static void onSlirpPollClose(uv_handle_t* handle);
	static void onSlirpPollTimeout(uv_timer_t* timer);

	static slirp_ssize_t onSlirpWrite(const void* buf, size_t len, void* opaque);
	static void onSlirpGuestError(const char* msg, void* opaque);
	static int64_t onSlirpClockGetNs(void* opaque);
	static void* onSlirpTimerNew(SlirpTimerId id, void* cb_opaque, void* opaque);
	static void onSlirpTimerFree(void* timer, void* opaque);
	static void onSlirpTimerMod(void* timer, int64_t expire_time, void* opaque);
	static void onSlirpTimerExpired(uv_timer_t* timer);
	static void onSlirpRegisterFd(int fd, void* opaque);
	static void onSlirpUnregisterFd(int fd, void* opaque);
	static void onSlirpNotify(void* opaque);

private:
	Slirp* slirpHandle = nullptr;
	uv_prepare_t prepareHandle;
	uv_timer_t pollTimerHandle;

	struct FdInfo {
		int fd;
		int events;
		int activeUvEvents;
		uv_poll_t pollHandle;
		SlirpServer* connection;
	};
	std::unordered_map<int, std::unique_ptr<FdInfo>> fdsToPoll;
	std::unordered_set<int> activeFds;
	bool updateSlirpPoll = true;

	struct SlirpTimer {
		uv_timer_t timerHandle;
		SlirpTimerId id;
		void* cb_opaque;
		SlirpServer* pipeConnection;
	};

	ISlirpClient* slirpClient = nullptr;
};
