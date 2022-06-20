// SPDX-License-Identifier: MIT

#include "PipeServer.h"
#include "PipeConnection.h"
#include <algorithm>
#include <libslirp.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <spdlog/spdlog.h>
#include <uv.h>

PipeServer::PipeServer(SlirpServer* slirpServer) : slirpServer(slirpServer) {
	uv_pipe_init(uv_default_loop(), &pipeHandle, 0);
	pipeHandle.data = this;
}

void PipeServer::listenPipe(const char* pipePath) {
	int result;
	this->pipePath = pipePath;

	SPDLOG_INFO("Listening SLIP on pipe {}", pipePath);

	result = uv_pipe_bind(&pipeHandle, pipePath);
	if(result < 0) {
		SPDLOG_ERROR("failed to bind to path {}: {} ({})", pipePath, uv_strerror(result), result);
		return;
	}
	result = uv_listen((uv_stream_t*) &pipeHandle, 1, &PipeServer::onConnection);
	if(result < 0) {
		SPDLOG_ERROR("failed to listen on path {}: {} ({})", pipePath, uv_strerror(result), result);
		return;
	}
}

void PipeServer::onConnection(uv_stream_t* server, int status) {
	PipeServer* thisInstance = (PipeServer*) server->data;

	if(status < 0) {
		SPDLOG_ERROR("failed to listen on path {}: {} ({})", thisInstance->pipePath, uv_strerror(status), status);
		return;
	}

	PipeConnection* pipeConnection = new PipeConnection(thisInstance->slirpServer);
	int result = uv_accept(server, (uv_stream_t*) pipeConnection->getHandle());
	if(result < 0) {
		delete pipeConnection;
	}

	SPDLOG_INFO("Got SLIP connection {} on SLIP pipe", (void*) pipeConnection);

	pipeConnection->setOnCloseCallback([pipeConnection]() {
		SPDLOG_INFO("SLIP Connection {} closed", (void*) pipeConnection);
		delete pipeConnection;
	});

	pipeConnection->startRead();
}
