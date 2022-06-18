#include <spdlog/async.h>
#include <spdlog/cfg/env.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <vector>

#include "PipeConnection.h"
#include "PipeServer.h"
#include "SlirpServer.h"
#include <uv.h>

void initializeSpdLog() {
	spdlog::init_thread_pool(8192, 1);
	std::vector<spdlog::sink_ptr> sinks;

	auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%7l%$] %25!!: %v");
	console_sink->set_level(spdlog::level::debug);
	sinks.push_back(console_sink);

	auto logger = std::make_shared<spdlog::async_logger>(
	    "server", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::overrun_oldest);
	//*/
	// auto logger = std::make_shared<spdlog::logger>("server", sinks.begin(), sinks.end());
	spdlog::set_default_logger(logger);
	spdlog::set_level(spdlog::level::info);

	spdlog::cfg::load_env_levels();

	spdlog::flush_every(std::chrono::seconds(10));
}

char* checkAndIncrementArgIndex(int argc, char** argv, int& i) {
	// Return nothing if there is no argument or the argument starts with --
	if(i + 1 >= argc || strncmp(argv[i + 1], "--", 2) == 0) {
		return nullptr;
	}

	i++;
	return argv[i];
}

int main(int argc, char** argv) {
	/**
	 * --listen <pipe or tcp>
	 * --connect <pipe or tcp>
	 * --network <ip/mask>
	 * --disable-host-access
	 * --forward <port:port>
	 */
	enum class GuestMode { SERVER, CLIENT };

	const char* const defaultEndpoint = "\\\\.\\pipe\\serial-port";

	GuestMode guestMode = GuestMode::CLIENT;
	const char* guestEndpoint = nullptr;
	bool disableHostAccess = false;
	std::vector<std::pair<uint16_t, uint16_t>> forwardedPorts;

	initializeSpdLog();

	SPDLOG_INFO("Start slirp_server version {}", SLIRP_SERVER_VERSION);

	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "--listen") == 0) {
			guestMode = GuestMode::SERVER;
			guestEndpoint = checkAndIncrementArgIndex(argc, argv, i);
		} else if(strcmp(argv[i], "--connect") == 0) {
			guestMode = GuestMode::CLIENT;
			guestEndpoint = checkAndIncrementArgIndex(argc, argv, i);
		} else if(strcmp(argv[i], "--disable-host-access") == 0) {
			disableHostAccess = true;
		} else if(strcmp(argv[i], "--debug") == 0) {
			spdlog::set_level(spdlog::level::debug);
		} else if(strcmp(argv[i], "--forward") == 0) {
			char* forwardedPort = checkAndIncrementArgIndex(argc, argv, i);
			long hostPort;
			long guestPort;
			char* numberEnd = nullptr;

			if(forwardedPort == nullptr) {
				SPDLOG_CRITICAL("forward requires hostport:guestport argument (ex: 18080:8080)");

				spdlog::shutdown();
				exit(1);
			}

			hostPort = strtol(forwardedPort, &numberEnd, 10);
			if(numberEnd == nullptr || *numberEnd != ':') {
				SPDLOG_CRITICAL("forward requires hostport:guestport argument (ex: 18080:8080), got {}", forwardedPort);

				spdlog::shutdown();
				exit(1);
			}

			guestPort = strtol(numberEnd + 1, &numberEnd, 10);
			if(numberEnd == nullptr || *numberEnd != '\0') {
				SPDLOG_CRITICAL("forward requires hostport:guestport argument (ex: 18080:8080), got {}", forwardedPort);

				spdlog::shutdown();
				exit(1);
			}

			if(hostPort <= 0 || hostPort >= 65536) {
				SPDLOG_CRITICAL("invalid host port number for foward argument: {}", hostPort);

				spdlog::shutdown();
				exit(1);
			}

			if(guestPort <= 0 || guestPort >= 65536) {
				SPDLOG_CRITICAL("invalid guest port number for foward argument: {}", hostPort);

				spdlog::shutdown();
				exit(1);
			}

			forwardedPorts.push_back(std::make_pair(uint16_t(hostPort), uint16_t(guestPort)));
		} else if(strcmp(argv[i], "--help") == 0) {
			SPDLOG_INFO("\nUsage: {} [options]\n"
			            "  --help                             Show this help\n"
			            "  --listen [<pipe>]                  Run in listen mode on given pipe\n"
			            "  --connect [<pipe>]                 Run in connect mode on given pipe\n"
			            "                                     (default mode)\n"
			            "  --disable-host-access              Disable access to host ports from guest\n"
			            "  --debug                            Show debug logs\n"
			            "  --forward <hostport>:<guestport>   Forward host port to guest (can be\n"
			            "                                     specified multiple times)\n"
			            "\n"
			            "Note: default pipe is {}\n",
			            argv[0],
			            defaultEndpoint);

			spdlog::shutdown();
			exit(1);
		}
	}

	if(guestEndpoint == nullptr) {
		guestEndpoint = defaultEndpoint;
	}

	SlirpServer slirpServer;
	PipeServer pipeServer(&slirpServer);
	PipeConnection pipeConnection(&slirpServer);

	slirpServer.init(disableHostAccess, forwardedPorts);

	if(guestMode == GuestMode::SERVER) {
		pipeServer.listenPipe(guestEndpoint);
	} else {
		pipeConnection.connectPipe(guestEndpoint);
	}

	uv_run(uv_default_loop(), UV_RUN_DEFAULT);

	spdlog::shutdown();

	return 0;
}
