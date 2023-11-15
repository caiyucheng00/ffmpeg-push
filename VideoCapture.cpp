#include "VideoCapture.h"

VideoCapture::VideoCapture()
{
}

VideoCapture::~VideoCapture()
{
	if (yuv_buf_) {
		delete[] yuv_buf_;
	}

	if (yuv_fp_) {
		fclose(yuv_fp_);
	}
}

RET_CODE VideoCapture::Init(const Properties& properties)
{
	video_test_ = properties.GetProperty("video_test", 0);
	input_yuv_name_ = properties.GetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
	x_ = properties.GetProperty("x", 0);
	y_ = properties.GetProperty("y", 0);
	width_ = properties.GetProperty("width", 1920);
	height_ = properties.GetProperty("height", 1080);
	pixel_format_ = properties.GetProperty("pixel_format", 0);
	fps_ = properties.GetProperty("fps", 25);
	frame_duration_ = 1000.0 / fps_;   //40ms

	if (openYuvFile(input_yuv_name_.c_str()) != 0)
	{
		printf("openYuvFile %s failed\n", input_yuv_name_.c_str());
		return RET_FAIL;
	}

	return RET_OK;
}

void VideoCapture::Loop()
{
	printf("VideoCapture::Loop() into\n");

	yuv_buf_size_ = (width_ + width_ % 2) * (height_ + height_ % 2) * 1.5;  //yuv420 必须偶数
	yuv_buf_ = new uint8_t[yuv_buf_size_];

	yuv_total_duration_ = 0;
	yuv_start_time_ = TimesUtil::GetTimeMillisecond();

	while (true)
	{
		if (abort_) {
			break;
		}
			
		if (readYuvFile(yuv_buf_, yuv_buf_size_) == 0)
		{
			if (is_first_frame_) {
				is_first_frame_ = false;
				printf("is_first_frame\n");
			}
			if (get_yuv_callback_)
			{
				get_yuv_callback_(yuv_buf_, yuv_buf_size_);
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	abort_ = false;
	closeYuvFile();
}

void VideoCapture::AddCallback(std::function<void(uint8_t*, int32_t)> callback)
{
	get_yuv_callback_ = callback;
}

int VideoCapture::openYuvFile(const char* file_name)
{
	yuv_fp_ = fopen(file_name, "rb");
	if (!yuv_fp_) {
		return -1;
	}
	return 0;
}

int VideoCapture::readYuvFile(uint8_t* yuv_buf, int32_t yuv_buf_size)
{
	int64_t cur_time = TimesUtil::GetTimeMillisecond(); // 毫秒
	int64_t diff = cur_time - yuv_start_time_;          // 目前经过的时间
	if (((int64_t)yuv_total_duration_) > diff) {
		return 1;    // 还没到读取新一帧
	}

	// 读取一帧数据 ，持续40ms
	size_t ret = fread(yuv_buf_, 1, yuv_buf_size, yuv_fp_);
	if (ret != yuv_buf_size) {
		ret = fseek(yuv_fp_, 0, SEEK_SET); //头部
		ret = fread(yuv_buf_, 1, yuv_buf_size, yuv_fp_);
		if (ret != yuv_buf_size) {
			return -1;
		}
	}

	yuv_total_duration_ += frame_duration_;

	return 0;
}

int VideoCapture::closeYuvFile()
{
	if (yuv_fp_) {
		fclose(yuv_fp_);
	}
	return 0;
}
