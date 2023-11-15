#include "AACEncoder.h"

AACEncoder::AACEncoder()
{
}

AACEncoder::~AACEncoder()
{
	if (codec_ctx_) {
		avcodec_free_context(&codec_ctx_);
	}
}

//************************************
// Method:    Init
// FullName:  AACEncoder::Init
// Access:    public 
// Returns:   RET_CODE
// Qualifier: 采样率 48000
//			  通道 2
//			  比特率 128 * 1024
//			  通道布局（通道 2）
// Parameter: const Properties & properties
//************************************
RET_CODE AACEncoder::Init(const Properties& properties)
{ 
	// 获取参数
	sample_rate_ = properties.GetProperty("sample_rate", 48000);
	bitrate_ = properties.GetProperty("bitrate", 128 * 1024);
	channels_ = properties.GetProperty("channels", 2);
	channel_layout_ = properties.GetProperty("channel_layout", (int)av_get_default_channel_layout(channels_));
	
	//编码器 5参数
	codec_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec_) {
		printf("avcodec_find_encoder failed\n");
		return RET_ERR_MISMATCH_CODE;
	}
	codec_ctx_ = avcodec_alloc_context3(codec_);
	if (!codec_ctx_) {
		printf("avcodec_alloc_context3 failed\n");
		return RET_ERR_OUTOFMEMORY;
	}
	codec_ctx_->channels = channels_;
	codec_ctx_->channel_layout = channel_layout_;
	codec_ctx_->sample_fmt = AV_SAMPLE_FMT_FLTP; 
	codec_ctx_->sample_rate = sample_rate_;
	codec_ctx_->bit_rate = bitrate_;

	int ret = avcodec_open2(codec_ctx_, codec_, NULL);
	if (ret < 0) {
		printf("avcodec_open2 failed\n");
		avcodec_free_context(&codec_ctx_);
		return RET_FAIL;
	}

	return RET_OK;
}

AVPacket* AACEncoder::Encode(AVFrame* frame, const int64_t pts, int flush, RET_CODE* frame_code, RET_CODE* packet_code)
{
	int ret = 0;
	if (!codec_ctx_) {
		*frame_code = RET_FAIL;
		return nullptr;
	}

	if (frame) { // 存在
		frame->pts = pts;
		ret = avcodec_send_frame(codec_ctx_, frame);
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
	}

	if (flush) {
		avcodec_flush_buffers(codec_ctx_);
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
	return nullptr;
}

RET_CODE AACEncoder::GetAdtsHeader(uint8_t* adts_header, int aac_length)
{
	uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
	switch (codec_ctx_->sample_rate)
	{
	case 96000: freqIdx = 0; break;
	case 88200: freqIdx = 1; break;
	case 64000: freqIdx = 2; break;
	case 48000: freqIdx = 3; break;
	case 44100: freqIdx = 4; break;
	case 32000: freqIdx = 5; break;
	case 24000: freqIdx = 6; break;
	case 22050: freqIdx = 7; break;
	case 16000: freqIdx = 8; break;
	case 12000: freqIdx = 9; break;
	case 11025: freqIdx = 10; break;
	case 8000: freqIdx = 11; break;
	case 7350: freqIdx = 12; break;
	default:
		printf("can't support sample_rate:%d\n");
		freqIdx = 4;
		break;
	}
	uint8_t ch_cfg = codec_ctx_->channels;
	uint32_t frame_length = aac_length + 7;
	adts_header[0] = 0xFF;
	adts_header[1] = 0xF1;
	adts_header[2] = ((codec_ctx_->profile) << 6) + (freqIdx << 2) + (ch_cfg >> 2);
	adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length >> 11));
	adts_header[4] = ((frame_length & 0x7FF) >> 3);
	adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
	adts_header[6] = 0xFC;

	return RET_OK;
}

AVCodecContext* AACEncoder::GetAVCodecContext()
{
	if (codec_ctx_) return codec_ctx_;

	return nullptr;
}

AVSampleFormat AACEncoder::GetFormat()
{
	return codec_ctx_->sample_fmt;
}

int AACEncoder::GetFrameSamples()
{
	return codec_ctx_->frame_size;
}

int AACEncoder::GetFrameBytes()
{
	return av_get_bytes_per_sample(codec_ctx_->sample_fmt) * codec_ctx_->channels * codec_ctx_->frame_size;
}

int AACEncoder::GetChannels()
{
	return codec_ctx_->channels;
}

int AACEncoder::GetChannelLayout()
{
	return codec_ctx_->channel_layout;
}

int AACEncoder::GetSampleRate()
{
	return codec_ctx_->sample_rate;
}


