#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <systemd/sd-daemon.h>
#include <unistd.h>

#include <iostream>

#include "common/result.h"

#include "common.h"
#include "dataplane.h"

cDataPlane dataPlane;

void handleSignal(int signalType)
{
	if (signalType == SIGINT)
	{
		YADECAP_LOG_INFO("signal: SIGINT\n");
		/// @todo: stop
	}
	else if (signalType == SIGPIPE)
	{
		YADECAP_LOG_INFO("signal: SIGPIPE\n");
	}
	else if (signalType == SIGABRT)
	{
		YADECAP_LOG_INFO("signal: SIGABRT\n");

		void* array[10];
		size_t size;

		// get void*'s for all entries on the stack
		size = backtrace(array, 10);

		// print out all the frames to stderr
		fprintf(stderr, "Error: signal %d:\n", signalType);
		backtrace_symbols_fd(array, size, STDERR_FILENO);
		exit(1);
	}
}

int main(int argc,
         char** argv)
{
#if RTE_PKTMBUF_HEADROOM != 256
	std::cout << "wrong dpdk config" << std::endl;
	return 1;
#endif

	int config = argc;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-d") == 0)
		{
			common::log::logPriority = common::log::TLOG_DEBUG;
		}
		else if (strcmp(argv[i], "-c") == 0)
		{
			config = i + 1;
		}
	}
	if (config >= argc)
	{
		std::cout << "usage: " << argv[0] << " [-d] -c <dataplane.conf>" << std::endl;
		return 1;
	}

	auto result = dataPlane.init(argv[0], argv[config]);
	if (result != eResult::success)
	{
		return 2;
	}

	/** @todo
	if (signal(SIGINT, handleSignal) == SIG_ERR)
	{
		return 3;
	}
	*/

	if (signal(SIGPIPE, handleSignal) == SIG_ERR)
	{
		return 3;
	}

	if (signal(SIGABRT, handleSignal) == SIG_ERR)
	{
		return 3;
	}

	sd_notify(0, "READY=1");

	dataPlane.start();
	dataPlane.join();

	return 0;
}
