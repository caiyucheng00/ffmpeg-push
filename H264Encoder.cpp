#include "H264Encoder.h"

H264Encoder::H264Encoder()
{
}

H264Encoder::~H264Encoder()
{
	if (codec_ctx_) {
		avcodec_free_context(&codec_ctx_);
	}
	if (frame_) {
		av_frame_free(&frame_);
	}
}

//************************************
// Method:    Init
// FullName:  H264Encoder::Init
// Access:    public 
// Returns:   RET_CODE
// Qualifier: width 宽
//			  height 高
//			  fps  帧率
//            b_frame 直播为0
//			  bit_rate 比特率
//            gop
//            pix_fmt
// Parameter: const Properties & properties
//************************************
RET_CODE H264Encoder::Init(const Properties& properties)
{
	int ret = 0;
	// 获取参数
	width_ = properties.GetProperty("width", 0);
	height_ = properties.GetProperty("height", 0);
	if (width_ == 0 || width_ % 2 != 0 || height_ == 0 || height_ % 2 != 0) {
		printf("width: %d height: %d\n", width_, height_);
		return RET_ERR_NOT_SUPPORT;
	}
	fps_ = properties.GetProperty("fps", 25);
	b_frames_ = properties.GetProperty("b_frames", 0);
	bitrate_ = properties.GetProperty("bitrate", 500 * 1024);
	gop_ = properties.GetProperty("gop", fps_);

	//编码器
	codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!codec_) {
		printf("avcodec_find_encoder failed\n");
		return RET_ERR_MISMATCH_CODE;
	}
	codec_ctx_ = avcodec_alloc_context3(codec_);
	if (!codec_ctx_) {
		printf("avcodec_alloc_context3 failed\n");
		return RET_ERR_OUTOFMEMORY;
	}
	//编码后的视频帧大小，以像素为单位。
	codec_ctx_->width = width_;
	codec_ctx_->height = height_;
	//编码后的码率：值越大越清晰，值越小越流畅。
	codec_ctx_->bit_rate = bitrate_;
	//每25帧插入一个I帧 25fps
	codec_ctx_->gop_size = gop_;
	//帧率的基本单位，time_base.num为时间线分子，time_base.den为时间线分母，帧率=分子/分母。
	codec_ctx_->time_base.num = 1;
	codec_ctx_->time_base.den = fps_;
	codec_ctx_->framerate.num = fps_;
	codec_ctx_->framerate.den = 1;
	//图像色彩空间的格式，采用什么样的色彩空间来表明一个像素点。
	codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
	//编码器编码的数据类型
	codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
	//两个非B帧之间允许出现多少个B帧数，设置0表示不使用B帧，没有编码延时。B帧越多，压缩率越高。
	codec_ctx_->max_b_frames = b_frames_;
	//extradata 有 sps pps信息
	codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	//dict
	av_dict_set(&dict_, "preset", "medium", 0);
	av_dict_set(&dict_, "tune", "zerolatency", 0);  // 没有延迟的输出
	av_dict_set(&dict_, "profile", "high", 0);

	ret = avcodec_open2(codec_ctx_, codec_, &dict_);
	if (ret < 0) {
		printf("avcodec_open2 failed\n");
	}

	// 读取sps pps信息
	if (codec_ctx_->extradata)
	{
		printf("extradata_size:%d\n", codec_ctx_->extradata_size);
		// 第一个为sps 7
		// 第二个为pps 8

		uint8_t* sps = codec_ctx_->extradata + 4;    // 直接跳到数据 00 00 00 01
		int sps_len = 0;
		uint8_t* pps = NULL;
		int pps_len = 0;
		uint8_t* data = codec_ctx_->extradata + 4;
		for (int i = 0; i < codec_ctx_->extradata_size - 4; ++i)
		{
			if (0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
			{
				pps = &data[i + 4]; // 下一个 00 00 00 01
				break;
			}
		}
		sps_len = int(pps - sps) - 4;   // 4是下一个00 00 00 01占用的字节
		pps_len = codec_ctx_->extradata_size - 4 * 2 - sps_len;
		sps_.append(sps, sps + sps_len);
		pps_.append(pps, pps + pps_len);
	}

	//Init frame
	frame_ = av_frame_alloc();
	frame_->width = codec_ctx_->width;
	frame_->height = codec_ctx_->height;
	frame_->format = codec_ctx_->pix_fmt;
	ret = av_frame_get_buffer(frame_, 0);
	
	return RET_OK;
}

AVPacket* H264Encoder::Encode(uint8_t* yuv, int size, int64_t pts, RET_CODE* frame_code, RET_CODE* packet_code)
{
	int ret = 0;
	*frame_code = RET_OK;
	*packet_code = RET_OK;
	int need_size = 0;
	if (yuv)
	{
		need_size = av_image_fill_arrays(frame_->data, frame_->linesize, yuv, (AVPixelFormat)frame_->format,
			frame_->width, frame_->height, 1);
		if (need_size != size) {
			return nullptr;
		}
		frame_->pts = pts;
		frame_->pict_type = AV_PICTURE_TYPE_NONE;
		ret = avcodec_send_frame(codec_ctx_, frame_);
	}
	else { // 刷新 空
		ret = avcodec_send_frame(codec_ctx_, NULL);
	}

	if (ret < 0)
		{
			if (ret == AVERROR(EAGAIN)) {  // frame没有空间，读取pkt
				*frame_code = RET_ERR_EAGAIN;
				return nullptr;
			}
			else if (ret = AVERROR_EOF) {
				*frame_code = RET_ERR_EOF;
				return nullptr;
			}
			else {
				*frame_code = RET_FAIL;
				return nullptr;
			}
		}
	else
	{
		*frame_code = RET_OK;
	}

	AVPacket* packet = av_packet_alloc();
	ret = avcodec_receive_packet(codec_ctx_, packet);
	if (ret < 0)
	{
		av_packet_free(&packet);
		if (ret == AVERROR(EAGAIN)) {  // 继续发frame，才能读取pkt
			*packet_code = RET_ERR_EAGAIN;
			return nullptr;
		}
		else if (ret = AVERROR_EOF) {  // 不能读取出pkt了
			*packet_code = RET_ERR_EOF;
			return nullptr;
		}
		else {
			*packet_code = RET_FAIL;
			return nullptr;
		}
	}
	else {
		*packet_code = RET_OK;
		return packet;
	}
}

uint8_t* H264Encoder::get_sps_data()
{
	return (uint8_t*)sps_.c_str();
}

AVCodecContext* H264Encoder::GetAVCodecContext()
{
	if (codec_ctx_) return codec_ctx_;

	return nullptr;
}

int H264Encoder::get_sps_size()
{
	return sps_.size();
}

uint8_t* H264Encoder::get_pps_data()
{
	return (uint8_t*)pps_.c_str();
}

int H264Encoder::GetFPS()
{
	return fps_;
}

int H264Encoder::get_pps_size()
{
	return pps_.size();
}
