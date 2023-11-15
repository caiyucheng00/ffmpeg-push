#include "CommonLooper.h"

CommonLooper::CommonLooper() :
	abort_(false),
	running_(false)
{
}

CommonLooper::~CommonLooper()
{
	Stop();
}

RET_CODE CommonLooper::Start()
{
	printf("Start into\n");
	thread_ = new std::thread(trampoline, this);
	if (!thread_) {
		printf("new std::thread failed");
		return RET_FAIL;
	}

	return RET_OK;
}

void CommonLooper::Stop()
{
	abort_ = true;
	if (thread_) {
		thread_->join();
		delete thread_;
		thread_ = NULL;
	}
}

bool CommonLooper::getRunning()
{
	return running_;
}

void CommonLooper::setRunning(bool running)
{
	running_ = running;
}

void* CommonLooper::trampoline(void* arg)
{
	printf("trampoline LOOP into\n");
	((CommonLooper*)arg)->setRunning(true);
	((CommonLooper*)arg)->Loop(); // Ö÷ÒªÑ­»·
	((CommonLooper*)arg)->setRunning(false);
	printf("trampoline LOOP leave\n");

	return NULL;
}
