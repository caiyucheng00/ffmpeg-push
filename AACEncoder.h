#ifndef __AACENCODER__H_
#define  __AACENCODER__H_

#include "Utils.h"
#include "Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
}

class AACEncoder
{
public:
	AACEncoder();
	virtual ~AACEncoder();

	RET_CODE Init(const Properties& properties);
	AVPacket* Encode(AVFrame* frame, const int64_t pts, int flush, RET_CODE* frame_code, RET_CODE* packet_code);

	RET_CODE GetAdtsHeader(uint8_t* adts_header, int aac_length);

	AVCodecContext* GetAVCodecContext();

	AVSampleFormat GetFormat();
	int GetFrameSamples();
	int GetFrameBytes();
	int GetChannels();
	int GetChannelLayout();
	int GetSampleRate();

private:
	int sample_rate_ = 48000;
	int channels_ = 2;
	int bitrate_ = 128 * 1024;
	int channel_layout_ = AV_CH_LAYOUT_STEREO;

	const AVCodec* codec_ = NULL;
	AVCodecContext* codec_ctx_ = NULL;
};

#endif