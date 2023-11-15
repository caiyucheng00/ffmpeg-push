#include "AudioCapture.h"

AudioCapture::AudioCapture()
{
}

AudioCapture::~AudioCapture()
{
	if (pcm_buf_) {
		delete[] pcm_buf_;
	}

	if (pcm_fp_) {
		fclose(pcm_fp_);
	}
}

RET_CODE AudioCapture::Init(const Properties& properties)
{
	audio_test_ = properties.GetProperty("audio_test", 0);
	input_pcm_name_ = properties.GetProperty("input_pcm_name", "");
	sample_rate_ = properties.GetProperty("sample_rate", 48000);
	channels_ = properties.GetProperty("channels", 2);
	byte_per_sample_ = properties.GetProperty("byte_per_sample", 2);
	nb_samples_ = properties.GetProperty("nb_samples", 1024);
	pcm_buf_size_ = byte_per_sample_ * channels_ * nb_samples_;  //4096字节为一帧
	pcm_buf_ = new uint8_t[pcm_buf_size_];
	if (!pcm_buf_) {
		return RET_ERR_OUTOFMEMORY;
	}

	if (openPcmFile(input_pcm_name_.c_str()) < 0) {
		printf("%s(%d) openPcmFile %s failed: \n", __FUNCTION__, __LINE__, input_pcm_name_.c_str());
		return RET_FAIL;
	}

	frame_duration_ = 1.0 * nb_samples_ / sample_rate_ * 1000; //毫秒 21.3 为一帧
	
	return RET_OK;
}

void AudioCapture::Loop()
{
	printf("AudioCapture::Loop() into\n");
	pcm_total_duration_ = 0;
	pcm_start_time_ = TimesUtil::GetTimeMillisecond(); // 开始时间

	while (true) {
		if (abort_) { // 标志位
			break;
		}

		if (readPcmFile(pcm_buf_, pcm_buf_size_) == 0) { // 一帧
			if (is_first_time_) {
				is_first_time_ = false;
				printf("is_first_time\n");
			}

			if (get_pcm_callback_) {
				get_pcm_callback_(pcm_buf_, pcm_buf_size_);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	abort_ = false;
	closePcmFile();
}

void AudioCapture::AddCallback(std::function<void(uint8_t*, int32_t)> callback)
{
	get_pcm_callback_ = callback;
}

int AudioCapture::openPcmFile(const char* file_name)
{
	pcm_fp_ = fopen(file_name, "rb");
	if (!pcm_fp_) {
		return -1;
	}
	return 0;
}

int AudioCapture::readPcmFile(uint8_t* pcm_buf, int32_t pcm_buf_size)
{
	int64_t cur_time = TimesUtil::GetTimeMillisecond(); // 毫秒
	int64_t diff = cur_time - pcm_start_time_;          // 目前经过的时间
	if (((int64_t)pcm_total_duration_) > diff) {        
		return 1;    // 还没到读取新一帧
	}

	// 读取一帧数据 4096字节，持续21.3ms
	size_t ret = fread(pcm_buf_, 1, pcm_buf_size, pcm_fp_);
	if (ret != pcm_buf_size) {
		ret = fseek(pcm_fp_, 0, SEEK_SET);
		ret = fread(pcm_buf_, 1, pcm_buf_size, pcm_fp_);
		if (ret != pcm_buf_size) {
			return -1;
		}
	}

	pcm_total_duration_ += frame_duration_;
	return 0;
}

int AudioCapture::closePcmFile()
{
	if (pcm_fp_) {
		fclose(pcm_fp_);
	}
	return 0;
}
