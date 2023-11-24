#include <iostream>
#include "PushWork.h"
#include "MessageList.h"

using namespace std;
#define RTSP_URL "rtsp://192.168.154.128:5544/live/livestream"

int main(int args, char* argv[]) {
	cout << "主函数运行" << endl;
	MessageList* message_list = new MessageList();

	PushWork pushwork(message_list);
	Properties properties;
	properties.SetProperty("audio_test", 1);
	properties.SetProperty("input_pcm_name", "48000_2_s16le.pcm");
	properties.SetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
	properties.SetProperty("mic_sample_rate", 48000);
	properties.SetProperty("mic_channels", 2);

	properties.SetProperty("video_test", 1);
	properties.SetProperty("input_yuv_name", "720x480_25fps_420p.yuv");
	properties.SetProperty("desktop_x", 0);
	properties.SetProperty("desktop_y", 0);
	properties.SetProperty("desktop_width", 720);
	properties.SetProperty("desktop_height", 480);
	properties.SetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
	properties.SetProperty("desktop_fps", 25);

	//rtsp:url + udp
	properties.SetProperty("rtsp_url", RTSP_URL);
	properties.SetProperty("rtsp_transport", "tcp");
	properties.SetProperty("rtsp_timeout", 3000);
	properties.SetProperty("max_queue_duration", 1000);

	if (pushwork.Init(properties) != RET_OK) {
		printf("PushWork Init failed\n");
		return -1;
	}

	int count = 0;
	Message msg;
	int ret = 0;
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		/*ret = message_list->Get(&msg, 1000);
		if (1 == ret) {
			switch (msg.what) {
			case MSG_RTSP_ERROR:
				printf("MSG_RTSP_ERROR error:%d\n", msg.arg1);
				break;
			case MSG_RTSP_QUEUE_DURATION:
				printf("MSG_RTSP_QUEUE_DURATION a:%d, v:%d\n", msg.arg1, msg.arg2);
				break;
			default:
				break;
			}
		}*/
		
		if (count++ > 10000) {
			printf("main break\n");
			break;
		}
	}

	//message_list->Abort();
	//delete message_list;

	cout << "主函数结束" << endl;
}