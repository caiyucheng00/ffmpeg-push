#include "AudioResample.h"

AudioResample::AudioResample()
{
}

AudioResample::~AudioResample()
{
}

RET_CODE AudioResample::Init(const Properties& properties)
{
	in_sample_rate_ = properties.GetProperty("in_sample_rate", 0);
	in_channels_ = properties.GetProperty("in_channels", 0);
	in_sample_fmt_ = (AVSampleFormat)properties.GetProperty("in_sample_fmt", AV_SAMPLE_FMT_S16);
	out_sample_rate_ = properties.GetProperty("out_sample_rate", 0);
	out_channels_ = properties.GetProperty("out_channels", 0);
	out_sample_fmt_ = (AVSampleFormat)properties.GetProperty("out_sample_fmt", AV_SAMPLE_FMT_FLTP);
	
	swr_ctx_ = swr_alloc();
	if (!swr_ctx_) {
		printf("Could not allocate Resample context\n");
		return RET_FAIL;
	}

	// 设置输入参数
	av_opt_set_int(swr_ctx_, "in_channel_layout", av_get_default_channel_layout(in_channels_), 0);
	av_opt_set_int(swr_ctx_, "in_sample_rate", in_sample_rate_, 0);
	av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", in_sample_fmt_, 0);

	// 设置输出参数
	av_opt_set_int(swr_ctx_, "out_channel_layout", av_get_default_channel_layout(out_channels_), 0);
	av_opt_set_int(swr_ctx_, "out_sample_rate", out_sample_rate_, 0);
	av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", out_sample_fmt_, 0);

	// 初始化 SwrContext
	if (swr_init(swr_ctx_) < 0) {
		printf("Failed to initialize the resampling context\n");
		swr_free(&swr_ctx_);
		return RET_FAIL;
	}

	// 分配 fltp_buf_ 的内存
	int nb_samples = 1024; // 示例值，根据实际需求修改
	// 计算所需的总样本数
	int total_samples = nb_samples * out_channels_;
	// 分配内存
	fltp_buf_ = new float[total_samples];
	if (!fltp_buf_) {
		printf("Failed to allocate memory for fltp_buf_\n");
		return RET_FAIL;
	}
	
	return RET_OK;
}

void AudioResample::DeInit()
{
	if (swr_ctx_) {
		swr_free(&swr_ctx_);
	}
	if (fltp_buf_) {
		delete[] fltp_buf_;
		fltp_buf_ = nullptr;
	}
}

int AudioResample::Convert(uint8_t* s16le, uint8_t* fltp, int nb_samples)
{
	int ret = 0;
	const uint8_t* in_samples[] = { s16le };
	uint8_t* out_samples[] = { fltp }; // 假设 audio_frame_ 已经分配内存

	ret = swr_convert(swr_ctx_, out_samples, nb_samples, in_samples, nb_samples);
	if (ret < 0) {
		printf("Audio conversion failed\n");
		return ret;
	}

	return ret;
}

float* AudioResample::GetBuffer()
{
	if (fltp_buf_) return fltp_buf_;
	return nullptr;
}
