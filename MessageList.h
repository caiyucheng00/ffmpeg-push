#ifndef __MESSAGELIST__H_
#define __MESSAGELIST__H_

#include <mutex>
#include <condition_variable>
#include <list>
extern "C"
{
#include "libavcodec/avcodec.h"
}

#define MSG_FLUSH	1
#define MSG_RTSP_ERROR	100
#define MSG_RTSP_QUEUE_DURATION	101	

typedef struct Message
{
	int what;
	int arg1;
	int arg2;
	void* obj;
	void (*free)(void* obj);
}Message;

class MessageList
{
public:
	MessageList();
	~MessageList();

	void Init(Message* message);

	int Push(Message* message);
	int Get(Message* message, int timeout);
	void Remove(int what);

	void Notify(int what);
	void Notify(int what, int arg1);
	void Notify(int what, int arg1, int arg2);
	void Notify(int what, int arg1, int arg2, void* obj, int obj_len);

	void Abort();
	void release();

private:
	int pushPrivate(Message* message);
	static void msg_obj_free(void* obj);

	std::mutex mutex_;
	std::condition_variable cond_;
	std::list<Message*> list_;

	bool abort_ = false;
};

#endif