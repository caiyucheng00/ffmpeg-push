#ifndef __PACKETQUEUE__H_
#define __PACKETQUEUE__H_

#include <mutex>
#include <condition_variable>
#include <queue>
#include "Utils.h"
extern "C"
{
#include "libavcodec/avcodec.h"
}

typedef struct packet_queue_stats
{
	int audio_nb_packets;   // ��Ƶ������
	int video_nb_packets;   // ��Ƶ������
	int audio_size;         // ��Ƶ�ܴ�С �ֽ�
	int video_size;         // ��Ƶ�ܴ�С �ֽ�
	int64_t audio_duration; //��Ƶ����ʱ��
	int64_t video_duration; //��Ƶ����ʱ��
}PacketQueueStats;

typedef struct packet
{
	AVPacket* pkt;
	MediaType media_type;
}Packet;

class PacketQueue
{
public:
	PacketQueue(double audio_frame_duration, double video_frame_duration);
	~PacketQueue();

	int Push(AVPacket* pkt, MediaType media_type);
	int pushPrivate(AVPacket* pkt, MediaType media_type);
	int Pop(AVPacket** pkt, MediaType& media_type);
	int PopWithTimeout(AVPacket** pkt, MediaType& media_type, int timeout);
	bool Empty();
	void Abort();
	int Drop(bool all, int64_t remain_max_duration);
	int64_t GetAudioDuration();
	int64_t GetVideoDuration();
	int GetAudioPackets();
	int GetVideoPackets();
	void GetStats(PacketQueueStats* stats);

private:
	std::mutex mutex_;
	std::condition_variable cond_;
	std::queue<Packet*> queue_;

	bool abort_ = false;

	// ͳ�����
	PacketQueueStats stats_;
	double audio_frame_duration_ = 23.21995649; // Ĭ��23.2ms 44.1khz  1024*1000ms/44100=23.21995649ms
	double video_frame_duration_ = 40;  // 40ms ��Ƶ֡��Ϊ25��  �� 1000ms/25=40ms
	// pts��¼
	bool is_first_audio_ = true;
	int64_t audio_front_pts_ = 0;
	int64_t audio_back_pts_ = 0;
	bool is_first_video_ = true;
	int64_t video_front_pts_ = 0;
	int64_t video_back_pts_ = 0;
};

#endif