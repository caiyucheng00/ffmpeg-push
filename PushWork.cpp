#include "PushWork.h"

PushWork::PushWork(MessageList* message_list) :
	message_list_(message_list)
{
}

PushWork::~PushWork()
{
	// 捕获 + 编码 + 推流
	if (audio_capture_) delete audio_capture_;
	if (video_capture_) delete video_capture_;
	if (aac_encoder_) delete aac_encoder_;
	if (h264_encoder_) delete h264_encoder_;
	if (fltp_buf_) av_free(fltp_buf_);
	if (audio_frame_) av_frame_free(&audio_frame_);
	if (rtsp_pusher_) delete rtsp_pusher_;
	printf("~PushWork()\n");
}

RET_CODE PushWork::Init(const Properties& properties)
{
	int ret = 0;
	// 音频test模式
	audio_test_ = properties.GetProperty("audio_test", 0);
	input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");
	// 麦克风采样属性
	mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
	mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	mic_channels_ = properties.GetProperty("mic_channels", 2);
	// 音频编码参数
	audio_sample_rate_ = properties.GetProperty("audio_sample_rate", mic_sample_rate_);
	audio_bitrate_ = properties.GetProperty("audio_bitrate", 128 * 1024);
	audio_channels_ = properties.GetProperty("audio_channels", mic_channels_);
	audio_ch_layout_ = av_get_default_channel_layout(audio_channels_);    // 由audio_channels_决定

	// 视频test模式
	video_test_ = properties.GetProperty("video_test", 0);
	input_yuv_name_ = properties.GetProperty("input_yuv_name", "input_1280_720_420p.yuv");
	// 桌面录制属性
	desktop_x_ = properties.GetProperty("desktop_x", 0);
	desktop_y_ = properties.GetProperty("desktop_y", 0);
	desktop_width_ = properties.GetProperty("desktop_width", 720);
	desktop_height_ = properties.GetProperty("desktop_height", 480);
	desktop_format_ = properties.GetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	desktop_fps_ = properties.GetProperty("desktop_fps", 25);
	// 视频编码属性
	video_width_ = properties.GetProperty("video_width", desktop_width_);     // 宽
	video_height_ = properties.GetProperty("video_height", desktop_height_);   // 高
	video_fps_ = properties.GetProperty("video_fps", desktop_fps_);             // 帧率
	video_gop_ = properties.GetProperty("video_gop", video_fps_);
	video_bitrate_ = properties.GetProperty("video_bitrate", 1024 * 1024);   // 先默认1M fixedme
	video_b_frames_ = properties.GetProperty("video_b_frames", 0);   // b帧数量

	//rtsp属性
	rtsp_url_ = properties.GetProperty("rtsp_url", "");
	rtsp_transport_ = properties.GetProperty("rtsp_transport", "");
	rtsp_timeout_ = properties.GetProperty("rtsp_timeout", 5000);
	max_queue_duration_ = properties.GetProperty("max_queue_duration", 500);

	// 设置音频编码器，先音频捕获初始化
	aac_encoder_ = new AACEncoder();
	if (!aac_encoder_)
	{
		printf("new AACEncoder() failed\n");
		return RET_FAIL;
	}
	Properties  aac_encoder_properties;
	aac_encoder_properties.SetProperty("sample_rate", audio_sample_rate_);
	aac_encoder_properties.SetProperty("channels", audio_channels_);
	aac_encoder_properties.SetProperty("bitrate", audio_bitrate_);        // 这里没有去设置采样格式
																		// 需要什么样的采样格式是从编码器读取出来的
	if (aac_encoder_->Init(aac_encoder_properties) != RET_OK)
	{
		printf("AACEncoder Init failed\n");
		return RET_FAIL;
	}

	int frame_bytes2 = 0;
	// 默认读取出来的数据是s16的，编码器需要的是fltp, 需要做重采样
	// 手动把s16转成fltp
	fltp_buf_size_ = av_samples_get_buffer_size(NULL, aac_encoder_->GetChannels(), //2 * 1024 * 4 = 8192
		aac_encoder_->GetFrameSamples(),
		(enum AVSampleFormat)aac_encoder_->GetFormat(), 1);
	fltp_buf_ = (uint8_t*)av_malloc(fltp_buf_size_);
	if (!fltp_buf_) {
		printf("fltp_buf_ av_malloc failed");
		return RET_ERR_OUTOFMEMORY;
	}

	audio_frame_ = av_frame_alloc();
	audio_frame_->format = aac_encoder_->GetFormat();
	audio_frame_->format = AV_SAMPLE_FMT_FLTP;
	audio_frame_->nb_samples = aac_encoder_->GetFrameSamples();
	audio_frame_->channels = aac_encoder_->GetChannels();
	audio_frame_->channel_layout = aac_encoder_->GetChannelLayout();
	frame_bytes2 = aac_encoder_->GetFrameBytes();
	if (fltp_buf_size_ != frame_bytes2) {
		printf("frame_bytes1:%d != frame_bytes2:%d", fltp_buf_size_, frame_bytes2);
		return RET_FAIL;
	}
	ret = av_frame_get_buffer(audio_frame_, 0);
	if (ret < 0) {
		printf("audio_frame_ av_frame_get_buffer failed");
		return RET_FAIL;
	}

	// 视频编码
	h264_encoder_ = new H264Encoder();
	Properties  h264_encoder_properties;
	h264_encoder_properties.SetProperty("width", video_width_);
	h264_encoder_properties.SetProperty("height", video_height_);
	h264_encoder_properties.SetProperty("fps", video_fps_);            // 帧率
	h264_encoder_properties.SetProperty("b_frames", video_b_frames_);
	h264_encoder_properties.SetProperty("bitrate", video_bitrate_);    // 码率
	h264_encoder_properties.SetProperty("gop", video_gop_);            // gop
	if (h264_encoder_->Init(h264_encoder_properties) != RET_OK)
	{
		printf("H264Encoder Init failed\n");
		return RET_FAIL;
	}

	// rtsp初始化
	rtsp_pusher_ = new RTSPPusher(message_list_);
	Properties rtsp_pusher_properties;
	rtsp_pusher_properties.SetProperty("url", rtsp_url_);
	rtsp_pusher_properties.SetProperty("rtsp_transport", rtsp_transport_);
	rtsp_pusher_properties.SetProperty("rtsp_timeout", rtsp_timeout_);
	rtsp_pusher_properties.SetProperty("max_queue_duration", max_queue_duration_);
	if (aac_encoder_) {
		rtsp_pusher_properties.SetProperty("audio_frame_duration", aac_encoder_->GetFrameSamples() / aac_encoder_->GetSampleRate() * 1000);
	}
	if(h264_encoder_){
		rtsp_pusher_properties.SetProperty("video_frame_duration", 1000 / h264_encoder_->GetFPS());
	}
	if (rtsp_pusher_->Init(rtsp_pusher_properties) != RET_OK) {
		printf("rtsp_pusher_->Init failed\n");
		return RET_FAIL;
	}
	// 创建音频流、音视频流
	if (h264_encoder_) {
		if (rtsp_pusher_->ConfigVideoStream(h264_encoder_->GetAVCodecContext()) != RET_OK) {
			printf("rtsp_pusher ConfigVideoSteam failed\n");
			return RET_FAIL;
		}
	}
	if (aac_encoder_) {
		if (rtsp_pusher_->ConfigAudioStream(aac_encoder_->GetAVCodecContext()) != RET_OK) {
			printf("rtsp_pusher ConfigAudioStream failed\n");
			return RET_FAIL;
		}
	}
	if (rtsp_pusher_->Connect() != RET_OK) {
		printf("rtsp_pusher Connect() failed\n");
		return RET_FAIL;
	}

	// 设置音频捕获
	audio_capture_ = new AudioCapture();
	Properties audio_capture_properties;
	audio_capture_properties.SetProperty("audio_test", 1);
	audio_capture_properties.SetProperty("input_pcm_name", input_pcm_name_);
	audio_capture_properties.SetProperty("channels", mic_channels_);
	audio_capture_properties.SetProperty("nb_samples", 1024); //编码器提供
	if (audio_capture_->Init(audio_capture_properties) != RET_OK)
	{
		printf("AudioCapture Init failed\n");
		return RET_FAIL;
	}

	audio_capture_->AddCallback(std::bind(&PushWork::pcmCallback, this, std::placeholders::_1, std::placeholders::_2));
	if (audio_capture_->Start() != RET_OK) {
		printf("AudioCapture Start failed\n");
		return RET_FAIL;
	}

	// 设置视频捕获
	video_capture_ = new VideoCapture();
	Properties  video_capture_properties;
	video_capture_properties.SetProperty("video_test", 1);
	video_capture_properties.SetProperty("input_yuv_name", input_yuv_name_);
	video_capture_properties.SetProperty("width", desktop_width_);
	video_capture_properties.SetProperty("height", desktop_height_);
	if (video_capture_->Init(video_capture_properties) != RET_OK)
	{
		printf("VideoCapturer Init failed\n");
		return RET_FAIL;
	}

	video_capture_->AddCallback(std::bind(&PushWork::yuvCallback, this, std::placeholders::_1, std::placeholders::_2));
	if (video_capture_->Start() != RET_OK)
	{
		printf("VideoCapturer Start failed\n");
		return RET_FAIL;
	}

	return RET_OK;
}

void PushWork::DeInit()
{
	if (audio_capture_) {
		audio_capture_->Stop();
		delete audio_capture_;
		audio_capture_ = NULL;
	}

	if (video_capture_) {
		video_capture_->Stop();
		delete video_capture_;
		video_capture_ = NULL;
	}
}

// 只支持2通道 s16交错模式 -> float planar格式
void s16le_convert_to_fltp(short* s16le, float* fltp, int nb_samples) {
	float* fltp_l = fltp;   // -1~1
	float* fltp_r = fltp + nb_samples;
	for (int i = 0; i < nb_samples; i++) {
		fltp_l[i] = s16le[i * 2] / 32768.0;     // 0 2 4
		fltp_r[i] = s16le[i * 2 + 1] / 32768.0;   // 1 3 5
	}
}
void PushWork::pcmCallback(uint8_t* pcm, int32_t size)
{
	//printf("pcmCallback size: %d\n", size);
	int ret = 0;
	if (!pcm_s16le_fp_)
	{
		pcm_s16le_fp_ = fopen("push_dump_s16le.pcm", "wb");
	}
	if (pcm_s16le_fp_)
	{
		// ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
		fwrite(pcm, 1, size, pcm_s16le_fp_);
		fflush(pcm_s16le_fp_);
	}
	// 这里就约定好，音频捕获的时候，采样点数和编码器需要的点数是一样的
	s16le_convert_to_fltp((short*)pcm, (float*)fltp_buf_, audio_frame_->nb_samples);

	ret = av_frame_make_writable(audio_frame_);
	if (ret < 0) {
		printf("av_frame_make_writable failed\n");
		return;
	}
	// 将fltp_buf_写入frame
	ret = av_samples_fill_arrays(audio_frame_->data,
		audio_frame_->linesize,
		fltp_buf_,
		audio_frame_->channels,
		audio_frame_->nb_samples,
		(AVSampleFormat)audio_frame_->format,
		0);
	if (ret < 0) {
		printf("av_samples_fill_arrays failed\n");
		return;
	}

	int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_audio_pts();
	RET_CODE frame_code = RET_OK;
	RET_CODE packet_code = RET_OK;
	AVPacket* packet = aac_encoder_->Encode(audio_frame_, pts, 0, &frame_code, &packet_code);
	if (frame_code == RET_OK && packet_code == RET_OK) {
		if (!aac_fp_) {
			aac_fp_ = fopen("push_dump.aac", "wb");
			if (!aac_fp_) {
				printf("fopen push_dump.aac failed\n");
				return;
			}
		}
		if (aac_fp_) {
			uint8_t adts_header[7];
			if (aac_encoder_->GetAdtsHeader(adts_header, packet->size) != RET_OK) {
				printf("GetAdtsHeader failed\n");
				return;
			}
			fwrite(adts_header, 1, 7, aac_fp_);
			fwrite(packet->data, 1, packet->size, aac_fp_);
		}
	}
	printf("PcmCallback pts:%ld", pts);
	if (packet) {
		printf("PcmCallback packet->pts:%ld\n", packet->pts);
		rtsp_pusher_->Push(packet, E_AUDIO_TYPE);
	}
	else {
		printf("packet is null\n");
	}
}

void PushWork::yuvCallback(uint8_t* yuv, int32_t size)
{
	//printf("size: %d\n", size);
	int64_t pts = (int64_t)AVPublishTime::GetInstance()->get_video_pts();
	RET_CODE frame_code = RET_OK;
	RET_CODE packet_code = RET_OK;
	AVPacket* packet = h264_encoder_->Encode(yuv, size, pts, &frame_code, &packet_code);
	if (packet) {
		if (!h264_fp_) {
			h264_fp_ = fopen("push_dump.h264", "wb");
			if (!h264_fp_) {
				printf("fopen push_dump.h264 failed");
				return;
			}
			// 写入 sps pps
			uint8_t start_code[] = { 0,0,0,1 };
			fwrite(start_code, 1, 4, h264_fp_);
			fwrite(h264_encoder_->get_sps_data(), 1, h264_encoder_->get_sps_size(), h264_fp_);
			fwrite(start_code, 1, 4, h264_fp_);
			fwrite(h264_encoder_->get_pps_data(), 1, h264_encoder_->get_pps_size(), h264_fp_);
		}

		fwrite(packet->data, 1, packet->size, h264_fp_);
		fflush(h264_fp_);
	}
	printf("YuvCallback pts:%ld", pts);
	if (packet) {
		printf("YuvCallback packet->pts:%ld\n", packet->pts);
		rtsp_pusher_->Push(packet, E_VIDEO_TYPE);
	}
	else {
		printf("packet is null\n");
	}
}