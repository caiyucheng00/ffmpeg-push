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

	// pcm数据的数据回调
	void pcmCallback(uint8_t* pcm, int32_t size);
	// yuv数据的数据回调
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

	// 音频test模式
	int audio_test_ = 0;
	string input_pcm_name_;
	uint8_t* fltp_buf_ = NULL; // 重采样
	int fltp_buf_size_ = 0;
	// 麦克风采样属性
	int mic_sample_rate_ = 48000;
	int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
	int mic_channels_ = 2;
	// 音频编码参数
	int audio_sample_rate_ = AV_SAMPLE_FMT_S16;
	int audio_bitrate_ = 128 * 1024;
	int audio_channels_ = 2;
	int audio_sample_fmt_; // 具体由编码器决定，从编码器读取相应的信息
	int audio_ch_layout_;    // 由audio_channels_决定

	// 视频test模式
	int video_test_ = 0;
	string input_yuv_name_;
	// 桌面录制属性
	int desktop_x_ = 0;
	int desktop_y_ = 0;
	int desktop_width_ = 1920;
	int desktop_height_ = 1080;
	int desktop_format_ = AV_PIX_FMT_YUV420P;
	int desktop_fps_ = 25;
	// 视频编码属性
	int video_width_ = 1920;     // 宽
	int video_height_ = 1080;    // 高
	int video_fps_;             // 帧率
	int video_gop_;
	int video_bitrate_;
	int video_b_frames_;   // b帧数量

	FILE* h264_fp_ = NULL;
	FILE* aac_fp_ = NULL;
	FILE* pcm_s16le_fp_ = NULL;
};

#endif