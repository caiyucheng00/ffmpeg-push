#ifndef __PUSHWORK__H_
#define __PUSHWORK__H_

#include "AudioCapture.h"
#include "AudioResample.h"
#include "VideoCapture.h"
#include "AACEncoder.h"
#include "H264Encoder.h"
#include "AvTimeBase.h"
#include "RTSPPusher.h"
#include "MessageList.h"
#include "Utils.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

class PushWork
{
public:
	PushWork(MessageList* message_list);
	~PushWork();

	RET_CODE Init(const Properties& properties);
	void DeInit();

private:
	MessageList* message_list_ = NULL;

	// pcm���ݵ����ݻص�
	void pcmCallback(uint8_t* pcm, int32_t size);
	// yuv���ݵ����ݻص�
	void yuvCallback(uint8_t* yuv, int32_t size);

	AudioCapture* audio_capture_ = NULL;
	VideoCapture* video_capture_ = NULL;

	AudioResample* audio_resample_ = NULL;

	AACEncoder* aac_encoder_ = NULL;
	H264Encoder* h264_encoder_ = NULL;
	AVFrame* audio_frame_ = NULL;

	RTSPPusher* rtsp_pusher_ = NULL;
	std::string rtsp_url_;
	std::string rtsp_transport_;
	int rtsp_timeout_ = 0;
	int max_queue_duration_ = 0;

	// ��Ƶtestģʽ
	int audio_test_ = 0;
	string input_pcm_name_;
	uint8_t* fltp_buf_ = NULL; // �ز���
	int fltp_buf_size_ = 0;
	// ��˷��������
	int mic_sample_rate_ = 48000;
	int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
	int mic_channels_ = 2;
	// ��Ƶ�������
	int audio_sample_rate_ = AV_SAMPLE_FMT_S16;
	int audio_bitrate_ = 128 * 1024;
	int audio_channels_ = 2;
	int audio_sample_fmt_; // �����ɱ������������ӱ�������ȡ��Ӧ����Ϣ
	int audio_ch_layout_;    // ��audio_channels_����

	// ��Ƶtestģʽ
	int video_test_ = 0;
	string input_yuv_name_;
	// ����¼������
	int desktop_x_ = 0;
	int desktop_y_ = 0;
	int desktop_width_ = 1920;
	int desktop_height_ = 1080;
	int desktop_format_ = AV_PIX_FMT_YUV420P;
	int desktop_fps_ = 25;
	// ��Ƶ��������
	int video_width_ = 1920;     // ��
	int video_height_ = 1080;    // ��
	int video_fps_;             // ֡��
	int video_gop_;
	int video_bitrate_;
	int video_b_frames_;   // b֡����

	FILE* h264_fp_ = NULL;
	FILE* aac_fp_ = NULL;
	FILE* pcm_s16le_fp_ = NULL;
};

#endif