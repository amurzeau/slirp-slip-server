// SPDX-License-Identifier: MIT

#pragma once

#include <libslirp.h>
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <uv.h>
#include <vector>

class SlirpServer;

class PipeServer {
public:
	PipeServer(SlirpServer* slirpServer);

	void listenPipe(const char* pipePath);

private:
	// callbacks
	static void onConnection(uv_stream_t* server, int status);

private:
	SlirpServer* slirpServer;
	uv_pipe_t pipeHandle;
	std::string pipePath;
};
