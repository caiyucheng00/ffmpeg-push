#ifndef __RTSPPUSHER__H_
#define __RTSPPUSHER__H_

#include "CommonLooper.h"
#include "PacketQueue.h"
#include "timeutils.h"
#include "MessageList.h"
extern "C" {
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
}

class RTSPPusher : public CommonLooper
{
public:
	RTSPPusher(MessageList* message_list);
	virtual ~RTSPPusher();

	RET_CODE Init(const Properties& properties);
	void DeInit();
	RET_CODE Push(AVPacket* pkt, MediaType media_type);
	// 连接服务器，如果连接成功则启动线程
	RET_CODE Connect();
	// 超时
	bool IsTimeout();
	void ResetTime();
	int GetBlockTime();

	// 如果有视频成分
	RET_CODE ConfigVideoStream(const AVCodecContext* ctx);
	// 如果有音频成分
	RET_CODE ConfigAudioStream(const AVCodecContext* ctx);
	
	virtual void Loop();
private:
	int sendPacket(AVPacket* pkt, MediaType media_type);
	static int decode_interrupt_cb(void* arg);
	//监测队列
	void checkQueueDuration();

	MessageList* message_list_ = NULL;

	// 整个输出流的上下文
	AVFormatContext* fmt_ctx_ = NULL;
	// 视频编码器上下文
	AVCodecContext* video_codec_ctx_ = NULL;
	// 音频频编码器上下文
	AVCodecContext* audio_codec_ctx_ = NULL;

	// 流成分
	AVStream* video_stream_ = NULL;
	int video_index_ = -1;
	AVStream* audio_stream_ = NULL;
	int audio_index_ = -1;

	std::string url_ = "";
	std::string rtsp_transport_ = "";
	int rtsp_timeout_ = 0;

	double audio_frame_duration_ = 23.21995649; // 默认23.2ms 44.1khz  1024*1000ms/44100=23.21995649ms
	double video_frame_duration_ = 40;  // 40ms 视频帧率为25的  ， 1000ms/25=40ms
	PacketQueue* queue_ = NULL;
	int max_queue_duration_ = 500;    // 最大

	char err2str[256] = { 0 }; // error对应字符串
	// 处理超时
	int64_t time_out_ = 0;
	int64_t pre_time_ = 0;   // 记录调用前
};

#endif