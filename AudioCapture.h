#ifndef __AUDIOCAPTURE__H_
#define __AUDIOCAPTURE__H_

#include "CommonLooper.h"
#include "Utils.h"
#include <functional>
#include "timeutils.h"

class AudioCapture : public CommonLooper
{
public:
	AudioCapture();
	virtual ~AudioCapture();

	RET_CODE Init(const Properties& properties);
	virtual void Loop();
	
	void AddCallback(std::function<void(uint8_t*, int32_t)> callback);

private:
	int openPcmFile(const char* file_name);
	int readPcmFile(uint8_t* pcm_buf, int32_t pcm_buf_size);
	int closePcmFile();

	int audio_test_ = 0;
	std::string input_pcm_name_; // 输入pcm文件名 s16/2通道/48k
	FILE* pcm_fp_ = NULL;
	int64_t pcm_start_time_ = 0;
	double pcm_total_duration_ = 0;  // 推流的时长统计
	double frame_duration_ = 23.2;    // 1024 / 48000 = 21.3

	std::function<void(uint8_t*, int32_t)> get_pcm_callback_;
	uint8_t* pcm_buf_;
	int32_t pcm_buf_size_;
	bool is_first_time_ = true;
	int sample_rate_ = 48000;
	int nb_samples_ = 1024;
	int format_ = 1;    // s16
	int channels_ = 2;
	int byte_per_sample_ = 2;
};

#endif