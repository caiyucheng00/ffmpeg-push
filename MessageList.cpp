#include "MessageList.h"

MessageList::MessageList()
{
}

MessageList::~MessageList()
{
	release();
}

void MessageList::Init(Message* message)
{
	memset(message, 0, sizeof(Message));
}

int MessageList::Push(Message* message)
{
	std::lock_guard<std::mutex> lock(mutex_);
	int ret = pushPrivate(message);
	if (0 == ret) {
		cond_.notify_one();     // 正常插入队列了才会notify
	}

	return ret;
}

int MessageList::Get(Message* message, int timeout)
{
	if (!message) {
		return -2;
	}
	std::unique_lock<std::mutex> lock(mutex_);
	Message* msg;
	int ret;
	while(true) {
		if (abort_) {
			ret = -1;
			break;
		}
		if (!list_.empty()) {
			msg = list_.front();
			*message = *msg;
			list_.pop_front();
			av_free(msg);      // 释放msg1
			ret = 1;
			break;
		}
		else if (0 == timeout) {
			ret = 0;        // 没有消息
			break;
		}
		else if (timeout < 0) {
			cond_.wait(lock, [this] {
				return !list_.empty() | abort_;    // 队列不为空或者abort请求才退出wait
				});
		}
		else if (timeout > 0) {
			cond_.wait_for(lock, std::chrono::milliseconds(timeout), [this] {
				return !list_.empty() | abort_;        // 直接写return true;是错误的
				});
			if (list_.empty()) {
				ret = 0;
				break;
			}
		}
		else {
			ret = -2;
			break;
		}
	}
	return ret;
}

void MessageList::Remove(int what)
{
	std::lock_guard<std::mutex> lock(mutex_);
	while (!abort_ && !list_.empty()) {
		std::list<Message*>::iterator it;
		Message* msg = NULL;
		for (it = list_.begin(); it != list_.end(); it++) {
			if ((*it)->what == what) {
				msg = *it;
				break;
			}
		}
		if (msg) {
			if (msg->obj && msg->free) {
				msg->free(msg->obj);
			}
			av_free(msg);
			list_.remove(msg);
		}
		else {
			break;
		}
	}
}

void MessageList::Notify(int what)
{
	Message msg;
	Init(&msg);
	msg.what = what;
	Push(&msg);
}

void MessageList::Notify(int what, int arg1)
{
	Message msg;
	Init(&msg);
	msg.what = what;
	msg.arg1 = arg1;
	Push(&msg);
}

void MessageList::Notify(int what, int arg1, int arg2)
{
	Message msg;
	Init(&msg);
	msg.what = what;
	msg.arg1 = arg1;
	msg.arg2 = arg2;
	Push(&msg);
}

void MessageList::Notify(int what, int arg1, int arg2, void* obj, int obj_len)
{
	Message msg;
	Init(&msg);
	msg.what = what;
	msg.arg1 = arg1;
	msg.arg2 = arg2;
	msg.obj = av_malloc(obj_len);
	msg.free = msg_obj_free;
	memcpy(msg.obj, obj, obj_len);
	Push(&msg);
}

void MessageList::Abort()
{
	std::lock_guard<std::mutex> lock(mutex_);
	abort_ = true;
}

void MessageList::release()
{
	std::lock_guard<std::mutex> lock(mutex_);
	while (!list_.empty()) {
		Message* msg = list_.front();
		if (msg->obj && msg->free) {
			msg->free(msg->obj);
		}
		list_.pop_front();
		av_free(msg);
	}
}

int MessageList::pushPrivate(Message* message)
{
	if (abort_) {
		return -1;
	}
	Message* msg = (Message*)av_malloc(sizeof(Message));
	if (!msg) {
		return -1;
	}
	*msg = *message;
	list_.push_back(msg);
	return 0;
}

void MessageList::msg_obj_free(void* obj)
{
	av_free(obj);
}
