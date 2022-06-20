// SPDX-License-Identifier: MIT

#pragma once

class ISlirpClient {
public:
	virtual ~ISlirpClient() {}
	virtual void close() = 0;
	virtual void sendSlirpPacketToGuest(const void* data, size_t len) = 0;
};
