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

class SlirpServer;

class PipeConnection : public ISlirpClient {
public:
	PipeConnection(SlirpServer* slirpServer);
	virtual ~PipeConnection();
	void connectPipe(const char* pipePath);
	void startRead();
	void close();
	void setOnCloseCallback(std::function<void()> onCloseFunction) { this->onCloseFunction = onCloseFunction; }

	virtual void sendSlirpPacketToGuest(const void* data, size_t len) override;

	uv_pipe_t* getHandle() { return &pipeHandle; }

private:
	// functions
	void resetInputBuffer();

private:
	// callbacks

	static void onConnectedStatic(uv_connect_t* req, int status) {
		((PipeConnection*) req->data)->onConnected(req, status);
	}
	void onConnected(uv_connect_t* req, int status);

	static void onAllocStatic(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
		((PipeConnection*) handle->data)->onAlloc(handle, suggested_size, buf);
	}
	void onAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);

	static void onReadStatic(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
		((PipeConnection*) stream->data)->onRead(stream, nread, buf);
	}
	void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);

	static void onWriteStatic(uv_write_t* req, int status) { ((PipeConnection*) req->data)->onWrite(req, status); }
	void onWrite(uv_write_t* req, int status);

	static void onCloseStatic(uv_handle_t* handle) { ((PipeConnection*) handle->data)->onClose(handle); }
	void onClose(uv_handle_t* handle);

private:
	struct WriteBuffer {
		uv_write_t writeReq;
		uv_buf_t buf;
		std::vector<uint8_t> data;
	};

	SlirpServer* slirpServer;
	uv_pipe_t pipeHandle;
	uv_connect_t connectReq;
	std::string pipePath;

	const static uint8_t END = 0xC0;      // Indicates the end of a packet.
	const static uint8_t ESC = 0xDB;      // Indicates byte stuffing.
	const static uint8_t ESC_END = 0xDC;  // ESC ESC_END means END data byte.
	const static uint8_t ESC_ESC = 0xDD;  // ESC ESC_ESC means ESC data byte.
	std::vector<uint8_t> inputBuffer;
	bool escapeNext = false;

	std::function<void()> onCloseFunction;
};
