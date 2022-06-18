#include "PipeConnection.h"
#include "SlirpServer.h"
#include <algorithm>
#include <libslirp.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#include <uv.h>

PipeConnection::PipeConnection(SlirpServer* slirpServer) : slirpServer(slirpServer) {
	uv_pipe_init(uv_default_loop(), &pipeHandle, 0);
	pipeHandle.data = this;

	connectReq = {};
	connectReq.data = this;

	resetInputBuffer();
}

PipeConnection::~PipeConnection() {
	slirpServer->detachClient(this);
}

void PipeConnection::connectPipe(const char* pipePath) {
	this->pipePath = pipePath;

	SPDLOG_INFO("Connecting to SLIP pipe {}", pipePath);

	uv_pipe_connect(&connectReq, &pipeHandle, pipePath, &PipeConnection::onConnectedStatic);
}

void PipeConnection::startRead() {
	slirpServer->attachClient(this);

	uv_read_start((uv_stream_t*) &pipeHandle, &PipeConnection::onAllocStatic, &PipeConnection::onReadStatic);
}

void PipeConnection::close() {
	uv_read_stop((uv_stream_t*) &pipeHandle);
	uv_close((uv_handle_t*) &pipeHandle, &PipeConnection::onCloseStatic);
}

void PipeConnection::resetInputBuffer() {
	inputBuffer.clear();
	inputBuffer.assign(SlirpServer::SLIRP_ETHER_HEADER,
	                   SlirpServer::SLIRP_ETHER_HEADER + sizeof(SlirpServer::SLIRP_ETHER_HEADER));
}

void PipeConnection::onConnected(uv_connect_t* req, int status) {
	(void) req;

	if(status < 0) {
		SPDLOG_ERROR("failed to connect to {}, uv error: {} ({})", pipePath, uv_strerror(status), status);
		return;
	}

	SPDLOG_INFO("Connected to {}", pipePath);

	startRead();
}

void PipeConnection::onAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	(void) handle;

	buf->base = (char*) malloc(suggested_size);
	buf->len = (unsigned int) suggested_size;
}

void PipeConnection::onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
	if(nread < 0) {
		if(nread == UV_EOF) {
			SPDLOG_DEBUG("client disconnected");
		} else {
			int status = (int) nread;
			SPDLOG_ERROR("failed to read data, uv error: {} ({})", uv_strerror(status), status);
		}
		free(buf->base);
		close();
		return;
	}

	SPDLOG_TRACE("Received {} bytes from pipe: {:a}", nread, spdlog::to_hex(buf->base, buf->base + nread, 16));

	for(ssize_t i = 0; i < nread; i++) {
		uint8_t byte = buf->base[i];

		if(escapeNext) {
			if(byte == ESC_END) {
				byte = END;
			} else if(byte == ESC_ESC) {
				byte = ESC;
			}
			escapeNext = false;
		} else {
			if(byte == ESC) {
				escapeNext = true;
				continue;
			} else if(byte == END) {
				if(inputBuffer.size() > SlirpServer::SLIRP_ETHER_HEADER_SIZE) {
					slirpServer->receivePacketFromGuest(&inputBuffer[0], inputBuffer.size());
					resetInputBuffer();
				}
				continue;
			}
		}
		inputBuffer.push_back(byte);
	}

	free(buf->base);
}

void PipeConnection::onWrite(uv_write_t* req, int status) {
	WriteBuffer* writeBuffer = (WriteBuffer*) req->data;

	if(status < 0) {
		SPDLOG_ERROR("failed to write data, uv error: {} ({})", uv_strerror(status), status);
	}

	delete writeBuffer;
}

void PipeConnection::onClose(uv_handle_t* handle) {
	slirpServer->detachClient(this);
	if(onCloseFunction)
		onCloseFunction();
}

void PipeConnection::sendSlirpPacketToGuest(const void* buf, size_t len) {
	const uint8_t* bufToSend = ((const uint8_t*) buf);

	if(bufToSend[12] != 0x08 || bufToSend[13] != 0x00) {
		SPDLOG_ERROR("SLiRP try to send a non-IPv4 packet with EtherType {:x}", (bufToSend[12] << 8) | bufToSend[13]);
		return;
	}

	// Send without ethernet header
	bufToSend += SlirpServer::SLIRP_ETHER_HEADER_SIZE;
	len -= SlirpServer::SLIRP_ETHER_HEADER_SIZE;

	WriteBuffer* writeBuffer = new WriteBuffer;
	writeBuffer->writeReq.data = writeBuffer;

	writeBuffer->data.reserve(len + 10);
	writeBuffer->data.push_back(END);
	for(size_t i = 0; i < len; i++) {
		uint8_t byte = bufToSend[i];
		if(byte == END) {
			writeBuffer->data.push_back(ESC);
			writeBuffer->data.push_back(ESC_END);
		} else if(byte == ESC) {
			writeBuffer->data.push_back(ESC);
			writeBuffer->data.push_back(ESC_ESC);
		} else {
			writeBuffer->data.push_back(byte);
		}
	}
	writeBuffer->data.push_back(END);

	writeBuffer->buf = uv_buf_init((char*) &writeBuffer->data[0], (unsigned int) writeBuffer->data.size());

	uv_write(&writeBuffer->writeReq, (uv_stream_t*) &pipeHandle, &writeBuffer->buf, 1, &PipeConnection::onWriteStatic);
}
