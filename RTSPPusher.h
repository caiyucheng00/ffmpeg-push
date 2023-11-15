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
	// ���ӷ�������������ӳɹ��������߳�
	RET_CODE Connect();
	// ��ʱ
	bool IsTimeout();
	void ResetTime();
	int GetBlockTime();

	// �������Ƶ�ɷ�
	RET_CODE ConfigVideoStream(const AVCodecContext* ctx);
	// �������Ƶ�ɷ�
	RET_CODE ConfigAudioStream(const AVCodecContext* ctx);
	
	virtual void Loop();
private:
	int sendPacket(AVPacket* pkt, MediaType media_type);
	static int decode_interrupt_cb(void* arg);
	//������
	void checkQueueDuration();

	MessageList* message_list_ = NULL;

	// �����������������
	AVFormatContext* fmt_ctx_ = NULL;
	// ��Ƶ������������
	AVCodecContext* video_codec_ctx_ = NULL;
	// ��ƵƵ������������
	AVCodecContext* audio_codec_ctx_ = NULL;

	// ���ɷ�
	AVStream* video_stream_ = NULL;
	int video_index_ = -1;
	AVStream* audio_stream_ = NULL;
	int audio_index_ = -1;

	std::string url_ = "";
	std::string rtsp_transport_ = "";
	int rtsp_timeout_ = 0;

	double audio_frame_duration_ = 23.21995649; // Ĭ��23.2ms 44.1khz  1024*1000ms/44100=23.21995649ms
	double video_frame_duration_ = 40;  // 40ms ��Ƶ֡��Ϊ25��  �� 1000ms/25=40ms
	PacketQueue* queue_ = NULL;
	int max_queue_duration_ = 500;    // ���

	char err2str[256] = { 0 }; // error��Ӧ�ַ���
	// ����ʱ
	int64_t time_out_ = 0;
	int64_t pre_time_ = 0;   // ��¼����ǰ
};

#endif