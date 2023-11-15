#include "RTSPPusher.h"

RTSPPusher::RTSPPusher(MessageList* message_list) :
	message_list_(message_list)
{
}

RTSPPusher::~RTSPPusher()
{
	DeInit();
}

RET_CODE RTSPPusher::Init(const Properties& properties)
{
	int ret = 0;

	url_ = properties.GetProperty("url", "");
	rtsp_transport_ = properties.GetProperty("rtsp_transport", "");
	audio_frame_duration_ = properties.GetProperty("audio_frame_duration", 0);
	video_frame_duration_ = properties.GetProperty("video_frame_duration", 0);
	time_out_ = properties.GetProperty("rtsp_timeout", 5000);
	max_queue_duration_ = properties.GetProperty("max_queue_duration", 500);
	if (url_ == "") {
		printf("url is null\n");
		return RET_FAIL;
	}
	if (rtsp_transport_ == "") {
		printf("rtsp_transport is null, use udp or tcp\n");
		return RET_FAIL;
	}

	// 初始化网络库
	ret = avformat_network_init();
	if (ret < 0) {
		printf("avformat_network_init failed\n");
		return RET_FAIL;
	}
	// 分配AVFormatContext
	ret = avformat_alloc_output_context2(&fmt_ctx_, NULL, "rtsp", url_.c_str());
	if (ret < 0) {
		printf("avformat_alloc_output_context2 failed\n");
		return RET_FAIL;
	}
	ret = av_opt_set(fmt_ctx_->priv_data, "rtsp_transport", rtsp_transport_.c_str(), 0);
	if (ret < 0) {
		printf("av_opt_set failed\n");
		return RET_FAIL;
	}

	fmt_ctx_->interrupt_callback.callback = decode_interrupt_cb; // 设置超时
	fmt_ctx_->interrupt_callback.opaque = this;

	// 创建队列
	queue_ = new PacketQueue(audio_frame_duration_, video_frame_duration_);
	if (!queue_) {
		printf("new PacketQueue failed\n");
		return RET_ERR_OUTOFMEMORY;
	}

	return RET_OK;
}

void RTSPPusher::DeInit()
{
	if (queue_) {
		queue_->Abort();
	}

	Stop();

	if (fmt_ctx_) {
		avformat_free_context(fmt_ctx_);
		fmt_ctx_ = NULL;
	}
	if (queue_) {
		delete queue_;
		queue_ = NULL;
	}
}

RET_CODE RTSPPusher::Push(AVPacket* pkt, MediaType media_type)
{
	int ret = queue_->Push(pkt, media_type);
	if (ret < 0) {
		return RET_FAIL;
	}
	else {
		return RET_OK;
	}
}

RET_CODE RTSPPusher::Connect()
{
	if (!audio_stream_ && !video_stream_) {
		return RET_FAIL;
	}

	ResetTime();
	//连接服务器
	int ret = avformat_write_header(fmt_ctx_, NULL);
	if (ret < 0) {
		av_strerror(ret, err2str, sizeof(err2str));
		message_list_->Notify(MSG_RTSP_ERROR, ret);
		printf("avformat_write_header failed:%s\n", err2str);
		return RET_FAIL;
	}

	return Start(); // 启动线程LOOP
}

bool RTSPPusher::IsTimeout()
{
	if (TimesUtil::GetTimeMillisecond() - pre_time_ > time_out_) {
		return true;
	}

	return false;
}

void RTSPPusher::ResetTime()
{
	pre_time_ = TimesUtil::GetTimeMillisecond();   // 重置
}

int RTSPPusher::GetBlockTime()
{
	return TimesUtil::GetTimeMillisecond() - pre_time_;
}

RET_CODE RTSPPusher::ConfigVideoStream(const AVCodecContext* video_codec_ctx)
{
	if (!fmt_ctx_) {
		printf("fmt_ctx is null\n");
		return RET_FAIL;
	}
	if (!video_codec_ctx) {
		printf("ctx is null\n");
		return RET_FAIL;
	}

	// 添加视频流
	AVStream* video_stream = avformat_new_stream(fmt_ctx_, NULL);
	if (!video_stream) {
		printf("avformat_new_stream failed\n");
		return RET_FAIL;
	}
	video_stream->codecpar->codec_tag = 0;
	// 从编码器拷贝信息
	avcodec_parameters_from_context(video_stream->codecpar, video_codec_ctx);
	video_codec_ctx_ = (AVCodecContext*)video_codec_ctx;
	video_stream_ = video_stream;
	video_index_ = video_stream->index;       // 整个索引非常重要 fmt_ctx_根据index判别 音视频包
	return RET_OK;
}

RET_CODE RTSPPusher::ConfigAudioStream(const AVCodecContext* audio_codec_ctx)
{
	if (!fmt_ctx_) {
		printf("fmt_ctx is null\n");
		return RET_FAIL;
	}
	if (!audio_codec_ctx) {
		printf("ctx is null\n");
		return RET_FAIL;
	}

	// 添加音频流
	AVStream* audio_stream = avformat_new_stream(fmt_ctx_, NULL);
	if (!audio_stream) {
		printf("avformat_new_stream failed\n");
		return RET_FAIL;
	}
	audio_stream->codecpar->codec_tag = 0;
	// 从编码器拷贝信息
	avcodec_parameters_from_context(audio_stream->codecpar, audio_codec_ctx);
	audio_codec_ctx_ = (AVCodecContext*)audio_codec_ctx;
	audio_stream_ = audio_stream;
	audio_index_ = audio_stream->index;       // 整个索引非常重要 fmt_ctx_根据index判别 音视频包
	return RET_OK;
}

void RTSPPusher::Loop()
{
	printf("RTSPPusher::Loop() into\n");
	int ret = 0;
	AVPacket* pkt = NULL;
	MediaType media_type;
	while (true) {
		if (abort_) {
			printf("abort request\n");
			break;
		}
		//std::this_thread::sleep_for(std::chrono::seconds(5));
		checkQueueDuration();

		ret = queue_->PopWithTimeout(&pkt, media_type, 2000);
		if (0 == ret) {
			if (abort_) {
				printf("abort request\n");
				av_packet_free(&pkt);
				break;
			}
			switch (media_type) {
				case E_VIDEO_TYPE:  // 视频
					ret = sendPacket(pkt, media_type);
					if (ret < 0) {
						message_list_->Notify(MSG_RTSP_ERROR, ret);
						printf("send video Packet failed\n");
					}
					av_packet_free(&pkt);
					break;
				case E_AUDIO_TYPE:
					ret = sendPacket(pkt, media_type);
					if (ret < 0) {
						message_list_->Notify(MSG_RTSP_ERROR, ret);
						printf("send audio Packet failed\n");
					}
					av_packet_free(&pkt);
					break;
				default:
					break;
			}
		}
	}

	ResetTime();
	ret = av_write_trailer(fmt_ctx_);
	if (ret < 0) {
		printf("av_write_trailer failed\n");
		return;
	}
}

int RTSPPusher::sendPacket(AVPacket* pkt, MediaType media_type)
{
	AVRational dst_time_base;
	AVRational src_time_base = { 1, 1000 };      // 我们采集、编码 时间戳单位都是ms
	
	if (E_VIDEO_TYPE == media_type) {
		pkt->stream_index = video_index_;                  //新建的流去赋值
		dst_time_base = video_stream_->time_base;
	}
	else if (E_AUDIO_TYPE == media_type) {
		pkt->stream_index = audio_index_;
		dst_time_base = audio_stream_->time_base;
	}
	else {
		printf("unknown mediatype:%d\n", media_type);
		return -1;
	}

	pkt->pts = av_rescale_q(pkt->pts, src_time_base, dst_time_base);
	pkt->duration = 0;

	ResetTime();
	int ret = av_write_frame(fmt_ctx_, pkt);
	if (ret < 0) {
		av_strerror(ret, err2str, sizeof(err2str));
		message_list_->Notify(MSG_RTSP_ERROR, ret);
		printf("av_write_frame failed:%s\n", err2str);
		return -1;
	}

	return 0;
}

//************************************
// Method:    decode_interrupt_cb
// FullName:  RTSPPusher::decode_interrupt_cb
// Access:    private static 
// Returns:   int
// Qualifier: 超时回调 1 退出阻塞
// Parameter: void * arg
//************************************
int RTSPPusher::decode_interrupt_cb(void* arg)
{
	RTSPPusher* rtsp_pusher_ = (RTSPPusher*)arg;
	if (rtsp_pusher_->IsTimeout()) {
		printf("Timeout:%dms\n", rtsp_pusher_->GetBlockTime());
		return 1;
	}
	return 0;
}

void RTSPPusher::checkQueueDuration()
{
	PacketQueueStats stats;
	queue_->GetStats(&stats);
	if (stats.audio_duration > max_queue_duration_ || stats.video_duration > max_queue_duration_) {
		message_list_->Notify(MSG_RTSP_QUEUE_DURATION, stats.audio_duration, stats.video_duration);
		queue_->Drop(false, max_queue_duration_);
	}
}
