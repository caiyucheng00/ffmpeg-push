#ifndef __COMMONLOOPER__H_
#define __COMMONLOOPER__H_

#include <thread>
#include "Utils.h"
#include "Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

// 线程基类
class CommonLooper
{
public:
	CommonLooper();
	virtual ~CommonLooper();

	virtual RET_CODE Start();
	virtual void Stop();
	virtual void Loop() = 0;

	virtual bool getRunning();
	virtual void setRunning(bool running);

private:
	static void* trampoline(void* arg);

protected:
	std::thread* thread_ = NULL;
	bool abort_ = false;
	bool running_ = true;
};


#endif 