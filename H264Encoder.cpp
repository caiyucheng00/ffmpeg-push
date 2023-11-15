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
// Qualifier: width ��
//			  height ��
//			  fps  ֡��
//            b_frame ֱ��Ϊ0
//			  bit_rate ������
//            gop
//            pix_fmt
// Parameter: const Properties & properties
//************************************
RET_CODE H264Encoder::Init(const Properties& properties)
{
	int ret = 0;
	// ��ȡ����
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

	//������
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
	//��������Ƶ֡��С��������Ϊ��λ��
	codec_ctx_->width = width_;
	codec_ctx_->height = height_;
	//���������ʣ�ֵԽ��Խ������ֵԽСԽ������
	codec_ctx_->bit_rate = bitrate_;
	//ÿ25֡����һ��I֡ 25fps
	codec_ctx_->gop_size = gop_;
	//֡�ʵĻ�����λ��time_base.numΪʱ���߷��ӣ�time_base.denΪʱ���߷�ĸ��֡��=����/��ĸ��
	codec_ctx_->time_base.num = 1;
	codec_ctx_->time_base.den = fps_;
	codec_ctx_->framerate.num = fps_;
	codec_ctx_->framerate.den = 1;
	//ͼ��ɫ�ʿռ�ĸ�ʽ������ʲô����ɫ�ʿռ�������һ�����ص㡣
	codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
	//�������������������
	codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
	//������B֮֡��������ֶ��ٸ�B֡��������0��ʾ��ʹ��B֡��û�б�����ʱ��B֡Խ�࣬ѹ����Խ�ߡ�
	codec_ctx_->max_b_frames = b_frames_;
	//extradata �� sps pps��Ϣ
	codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	//dict
	av_dict_set(&dict_, "preset", "medium", 0);
	av_dict_set(&dict_, "tune", "zerolatency", 0);  // û���ӳٵ����
	av_dict_set(&dict_, "profile", "high", 0);

	ret = avcodec_open2(codec_ctx_, codec_, &dict_);
	if (ret < 0) {
		printf("avcodec_open2 failed\n");
	}

	// ��ȡsps pps��Ϣ
	if (codec_ctx_->extradata)
	{
		printf("extradata_size:%d\n", codec_ctx_->extradata_size);
		// ��һ��Ϊsps 7
		// �ڶ���Ϊpps 8

		uint8_t* sps = codec_ctx_->extradata + 4;    // ֱ���������� 00 00 00 01
		int sps_len = 0;
		uint8_t* pps = NULL;
		int pps_len = 0;
		uint8_t* data = codec_ctx_->extradata + 4;
		for (int i = 0; i < codec_ctx_->extradata_size - 4; ++i)
		{
			if (0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
			{
				pps = &data[i + 4]; // ��һ�� 00 00 00 01
				break;
			}
		}
		sps_len = int(pps - sps) - 4;   // 4����һ��00 00 00 01ռ�õ��ֽ�
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
	else { // ˢ�� ��
		ret = avcodec_send_frame(codec_ctx_, NULL);
	}

	if (ret < 0)
		{
			if (ret == AVERROR(EAGAIN)) {  // frameû�пռ䣬��ȡpkt
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
		if (ret == AVERROR(EAGAIN)) {  // ������frame�����ܶ�ȡpkt
			*packet_code = RET_ERR_EAGAIN;
			return nullptr;
		}
		else if (ret = AVERROR_EOF) {  // ���ܶ�ȡ��pkt��
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
