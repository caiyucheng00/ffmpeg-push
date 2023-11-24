#ifndef __AUDIORESAMPLE__H_
#define __AUDIORESAMPLE__H_

#include "Utils.h"
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

class AudioResample
{
public:
	AudioResample();
	~AudioResample();

	RET_CODE Init(const Properties& properties);
	void DeInit();

	int Convert(uint8_t* s16le, uint8_t* fltp, int nb_samples);

	float* GetBuffer();

private:
	SwrContext* swr_ctx_ = nullptr;
	float* fltp_buf_ = nullptr;

	int in_sample_rate_;
	int out_sample_rate_;
	int in_channels_;
	int out_channels_;
	AVSampleFormat in_sample_fmt_;
	AVSampleFormat out_sample_fmt_;

};

#endif