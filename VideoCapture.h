#ifndef __VIDEOCAPTURE__H_
#define __VIDEOCAPTURE__H_

#include "CommonLooper.h"
#include "Utils.h"
#include <functional>
#include "timeutils.h"

class VideoCapture : public CommonLooper
{
public:
	VideoCapture();
	virtual ~VideoCapture();

	RET_CODE Init(const Properties& properties);
	virtual void Loop();

	void AddCallback(std::function<void(uint8_t*, int32_t)> callback);

private:
	int openYuvFile(const char* file_name);
	int readYuvFile(uint8_t* yuv_buf, int32_t yuv_buf_size);
	int closeYuvFile();

	int video_test_ = 0;
	std::string input_yuv_name_; 
	int x_;
	int y_;
	int width_ = 0;
	int height_ = 0;
	FILE* yuv_fp_ = NULL;
	int64_t yuv_start_time_ = 0;
	double yuv_total_duration_ = 0;  // 推流的时长统计
	double frame_duration_ = 40;    // fps=25
	int pixel_format_ = 0;
	int fps_;

	std::function<void(uint8_t*, int32_t)> get_yuv_callback_;
	uint8_t* yuv_buf_;
	int32_t yuv_buf_size_;
	bool is_first_frame_ = true;
};

#endif