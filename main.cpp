#include "main.h"
#include "VideoPlayer.h"
#include "VideoFrameQueue.h"
#include "AudioPlayer.h"


//返回上一帧的pts更新值(上一帧pts+流逝的时间)
double get_clock(play_clock_t* c) {
    return (c->pts_drift + av_gettime_relative() / 1000000.0);   // 展开得： c->pts + (time - c->last_updated)
}


void set_clock(play_clock_t* c, double pts) {
    double time = av_gettime_relative() / 1000000.0;
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
}


void init_clock(play_clock_t* c) {
    set_clock(c, NAN);
}


int main(int argc, char* argv[]) {
	//获取文件路径
	//char filepath[] = "./Debug/Audio_Video_Sync_Test.mp4";
    //char filepath[] = "./Debug/snow.mp4";

	char* filepath;
	if (argc==2) {
		filepath = argv[1];
	}
	else {
		printf("Usage: player <filename>");
		return -1;
	}

	player_stat_t* is;
    is = (player_stat_t*)av_mallocz(sizeof(player_stat_t));
    if (!is) {
        return -1;
    }
    init_clock(&is->video_clk);
    init_clock(&is->audio_clk);

	AudioPlayer* audioPlayer = new AudioPlayer(filepath, is);    
    VideoPlayer* videoPlayer = new VideoPlayer(filepath, is);

    audioPlayer->decode();
    audioPlayer->audio_playing();
    videoPlayer->video_playing();

    SDL_Event event;
    while (!is->flag_exit) {
        if (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {   //关闭窗口
                printf("[quit]\n");
                is->flag_exit = 1;
            }
            else if (event.type == SDL_KEYDOWN) {//键盘事件
                switch (event.key.keysym.sym) {
                    case SDLK_p:    //按p暂停，再按p恢复播放
                        if (is->flag_pause) printf("[continue]\n");
                        else printf("[pause]\n");
                        is->flag_pause = !is->flag_pause;
                        break;

                    case SDLK_1:    //按1前进10秒
                        printf("[forward 10s]\n");
                        is->forward = 10;
                        break;

                    case SDLK_3:    //按3前进30秒
                        printf("[forward 30s]\n");
                        is->forward = 30;
                        break;

                    case SDLK_u:    //按u音量+
                        printf("[volume up]\n");
                        audioPlayer->adjust_volume(VOLUME_UP);
                        break;

                    case SDLK_d:    //按d音量-
                        printf("[volume down]\n");
                        audioPlayer->adjust_volume(VOLUME_DOWN);
                        break;

                    case SDLK_f:    //按f全屏，再按取消
                        if (is->flag_fullscreen) printf("[window]\n");
                        else printf("[full screen]\n");
                        is->flag_fullscreen = !is->flag_fullscreen;
                        videoPlayer->do_fullscreen();
                        break;

                    case SDLK_q:    //按q(quick)加速
                        printf("[speed up]\n");
                        audioPlayer->adjust_speed(SPEED_UP);
                        break;

                    case SDLK_s:    //按s(slow)减速
                        printf("[speed down]\n");
                        audioPlayer->adjust_speed(SPEED_DOWN);
                        break;
                }
            }
            else if (event.type == SDL_WINDOWEVENT) {   //缩放窗口
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    printf("[resize window]\n");
                    videoPlayer->resize_window(event.window.data1, event.window.data2);
                }
            }

        }
    }

    //system("pause");
    return 0;
}