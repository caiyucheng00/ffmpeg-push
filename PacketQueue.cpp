#include "PacketQueue.h"

PacketQueue::PacketQueue(double audio_frame_duration, double video_frame_duration) :
	audio_frame_duration_(audio_frame_duration),
	video_frame_duration_(video_frame_duration)
{
	if (audio_frame_duration_ < 0) audio_frame_duration_ = 0;
	if (video_frame_duration_ < 0) video_frame_duration_ = 0;
	memset(&stats_, 0, sizeof(PacketQueueStats));
}

PacketQueue::~PacketQueue()
{
}

int PacketQueue::Push(AVPacket* pkt, MediaType media_type)
{
	if (!pkt) {
		printf("pkt is null\n");
		return -1;
	}
	if (media_type != E_AUDIO_TYPE && media_type != E_VIDEO_TYPE) {
		printf("media_type:%d is unknown\n", media_type);
		return -1;
	}

	std::lock_guard<std::mutex> lock(mutex_);
	int ret = pushPrivate(pkt, media_type);
	if (ret < 0) {
		printf("pushPrivate failed\n");
		return -1;
	}
	else {
		cond_.notify_one();
		return 0;
	}
}

int PacketQueue::pushPrivate(AVPacket* pkt, MediaType media_type)
{
	if (abort_) {
		printf("abort request\n");
		return -1;
	}

	Packet* packet = (Packet*)malloc(sizeof(Packet));
	if (!packet) {
		printf("malloc Packet failed\n");
		return -1;
	}

	packet->pkt = pkt;
	packet->media_type = media_type;
	if (E_AUDIO_TYPE == media_type) {
		stats_.audio_nb_packets++;      // ������
		stats_.audio_size += pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		audio_back_pts_ = pkt->pts;
		if (is_first_audio_) {  // ��ʼ����һ֡
			audio_back_pts_ = pkt->pts;
			is_first_audio_ = false;
		}
	}
	if (E_VIDEO_TYPE == media_type) {
		stats_.video_nb_packets++;      // ������
		stats_.video_size += pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		video_back_pts_ = pkt->pts;
		if (is_first_video_) { // ��ʼ����һ֡
			video_front_pts_ = pkt->pts;
			is_first_video_ = false;
		}
	}
	
	queue_.push(packet);     // һ��Ҫpush
	
	return 0;
}

int PacketQueue::Pop(AVPacket** pkt, MediaType& media_type)
{
	if (!pkt) {
		printf("pkt is null\n");
		return -1;
	}
	std::unique_lock<std::mutex> lock(mutex_);
	if (abort_) {
		printf("abort request\n");
		return -1;
	}
	if (queue_.empty()) {        // �ȴ�����
		// return�������false������wait, �������true�˳�wait
		cond_.wait(lock, [this] {
			return !queue_.empty() | abort_;
			});
	}
	if (abort_) {
		printf("abort request\n");
		return -1;
	}

	// �����ɻ�
	Packet* packet = queue_.front(); //��ȡ�����ײ�Ԫ�أ����ﻹû������������
	*pkt = packet->pkt;
	media_type = packet->media_type;

	if (E_AUDIO_TYPE == media_type) {
		stats_.audio_nb_packets--;      // ������
		stats_.audio_size -= packet->pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		audio_front_pts_ = packet->pkt->pts;
	}
	if (E_VIDEO_TYPE == media_type) {
		stats_.video_nb_packets--;      // ������
		stats_.video_size -= packet->pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		video_front_pts_ = packet->pkt->pts;
	}

	queue_.pop();
	free(packet);

	return 1;
}

int PacketQueue::PopWithTimeout(AVPacket** pkt, MediaType& media_type, int timeout)
{
	if (timeout < 0) {
		return Pop(pkt, media_type);
	}

	std::unique_lock<std::mutex> lock(mutex_);
	if (abort_) {
		printf("abort request\n");
		return -1;
	}
	if (queue_.empty()) {        // �ȴ�����
		// return�������false������wait, �������true�˳�wait
		cond_.wait_for(lock, std::chrono::milliseconds(timeout), [this] {
			return !queue_.empty() | abort_;
			});
	}
	if (abort_) {
		printf("abort request\n");
		return -1;
	}
	// �����ɻ�
	Packet* packet = queue_.front(); //��ȡ�����ײ�Ԫ�أ����ﻹû������������
	*pkt = packet->pkt;
	media_type = packet->media_type;

	if (E_AUDIO_TYPE == media_type) {
		stats_.audio_nb_packets--;      // ������
		stats_.audio_size -= packet->pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		audio_front_pts_ = packet->pkt->pts;
	}
	if (E_VIDEO_TYPE == media_type) {
		stats_.video_nb_packets--;      // ������
		stats_.video_size -= packet->pkt->size;
		// ����ʱ����ôͳ�ƣ�������pkt->duration
		video_front_pts_ = packet->pkt->pts;
	}

	queue_.pop();
	free(packet);
	return 0;
}

bool PacketQueue::Empty()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.empty();
}

void PacketQueue::Abort()
{
	std::lock_guard<std::mutex> lock(mutex_);
	abort_ = true;
	cond_.notify_all();
}

int PacketQueue::Drop(bool all, int64_t remain_max_duration)
{
	std::lock_guard<std::mutex> lock(mutex_);
	while (!queue_.empty()) {
		Packet* packet = queue_.front();
		if (!all && packet->media_type == E_VIDEO_TYPE && (packet->pkt->flags & AV_PKT_FLAG_KEY)) {
			int64_t duration = video_back_pts_ - video_front_pts_;  //��ptsΪ׼
			// Ҳ�ο�֡���������� *֡(��)��
			if (duration < 0     // pts����
				|| duration > video_frame_duration_ * stats_.video_nb_packets * 2) {
				duration = video_frame_duration_ * stats_.video_nb_packets;
			}
			printf("video duration:%lld\n", duration);
			if (duration <= remain_max_duration)
				break;          // ˵������break �˳�while
		}
		if (E_AUDIO_TYPE == packet->media_type) {
			stats_.audio_nb_packets--;      // ������
			stats_.audio_size -= packet->pkt->size;
			// ����ʱ����ôͳ�ƣ�������pkt->duration
			audio_front_pts_ = packet->pkt->pts;
		}
		if (E_VIDEO_TYPE == packet->media_type) {
			stats_.video_nb_packets--;      // ������
			stats_.video_size -= packet->pkt->size;
			// ����ʱ����ôͳ�ƣ�������pkt->duration
			video_front_pts_ = packet->pkt->pts;
		}
		av_packet_free(&packet->pkt);        // ���ͷ�AVPacket
		queue_.pop();
		free(packet);                        // ���ͷ�MyAVPacket
	}

	return 0;
}

int64_t PacketQueue::GetAudioDuration()
{
	std::lock_guard<std::mutex> lock(mutex_);
	int64_t duration = audio_back_pts_ - audio_front_pts_;  //��ptsΪ׼
	// Ҳ�ο�֡���������� *֡(��)��
	if (duration < 0     // pts����
		|| duration > audio_frame_duration_ * stats_.audio_nb_packets * 2) {
		duration = audio_frame_duration_ * stats_.audio_nb_packets;
	}
	else
	{
		duration += audio_frame_duration_;
	}
	return duration;
}

int64_t PacketQueue::GetVideoDuration()
{
	std::lock_guard<std::mutex> lock(mutex_);
	int64_t duration = video_back_pts_ - video_front_pts_;  //��ptsΪ׼
	// Ҳ�ο�֡���������� *֡(��)��
	if (duration < 0     // pts����
		|| duration > video_frame_duration_ * stats_.video_nb_packets * 2) {
		duration = video_frame_duration_ * stats_.video_nb_packets;
	}
	else {
		duration += video_frame_duration_;
	}
	return duration;
}

int PacketQueue::GetAudioPackets()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return stats_.audio_nb_packets;
}

int PacketQueue::GetVideoPackets()
{
	std::lock_guard<std::mutex> lock(mutex_);
	return stats_.video_nb_packets;
}

void PacketQueue::GetStats(PacketQueueStats* stats)
{
	if (!stats) {
		printf("stats is null\n");
		return;
	}
	std::lock_guard<std::mutex> lock(mutex_);

	int64_t audio_duration = audio_back_pts_ - audio_front_pts_;  //��ptsΪ׼
	// Ҳ�ο�֡���������� *֡(��)��
	if (audio_duration < 0     // pts����
		|| audio_duration > audio_frame_duration_ * stats_.audio_nb_packets * 2) {
		audio_duration = audio_frame_duration_ * stats_.audio_nb_packets;
	}
	else {
		audio_duration += audio_frame_duration_;
	}
	int64_t video_duration = video_back_pts_ - video_front_pts_;  //��ptsΪ׼
	// Ҳ�ο�֡���������� *֡(��)��
	if (video_duration < 0     // pts����
		|| video_duration > video_frame_duration_ * stats_.video_nb_packets * 2) {
		video_duration = video_frame_duration_ * stats_.video_nb_packets;
	}
	else {
		video_duration += video_frame_duration_;
	}

	stats->audio_duration = audio_duration;
	stats->video_duration = video_duration;
	stats->audio_nb_packets = stats_.audio_nb_packets;
	stats->video_nb_packets = stats_.audio_nb_packets;
	stats->audio_size = stats_.audio_size;
	stats->video_size = stats_.video_size;
}
