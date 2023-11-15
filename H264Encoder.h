#ifndef __H264ENCODER__H_
#define __H264ENCODER__H_

#include "Utils.h"
#include "Log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

}

class H264Encoder
{
public:
	H264Encoder();
	virtual ~H264Encoder();

	RET_CODE Init(const Properties& properties);
	AVPacket* Encode(uint8_t* yuv, int size, int64_t pts, RET_CODE* frame_code, RET_CODE* packet_code);

	AVCodecContext* GetAVCodecContext();

	uint8_t* get_sps_data(); 
	int get_sps_size();
	uint8_t* get_pps_data();
	int get_pps_size(); 

	int GetFPS();
		
private:
	std::string sps_;
	std::string pps_;
	int width_ = 0;
	int height_ = 0;
	int fps_ = 0;
	int b_frames_ = 0;
	int bitrate_ = 0;
	int gop_ = 0;
	bool annexb_ = false;
	int thread_ = 1;

	const AVCodec* codec_ = NULL;
	AVCodecContext* codec_ctx_ = NULL;
	AVDictionary* dict_ = NULL;
	AVFrame* frame_ = NULL;
};

#endif