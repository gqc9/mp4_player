#pragma once

#include "main.h"

#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


class VideoPlayer {
public:
	//VideoPlayer(player_stat_t* input_is);
	int video_player_init(char* filepath, int input_fps);
	int video_playing();

public:
	player_stat_t* is;

private:
	static int sfp_refresh_thread(void* opaque);
	int video_play_thread();
	int video_refresh(double* remaining_time);
	void display_one_frame();
	int video_decode_thread();
	int decode_frame(AVFrame* pFrame);
	int destroy();

	double vp_duration(frame_t* vp, frame_t* nextvp);
	double compute_target_delay(double delay);
	void update_video_pts(player_stat_t* is, double pts, int64_t pos, int serial);

private:
	std::shared_ptr<std::thread> m_pDecode1;
	std::shared_ptr<std::thread> m_pDecode2;

	AVFormatContext* pFormatCtx; //解封装
	AVCodecContext* pCodecCtx; //解码
	AVStream* pStream;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV; //帧数据
	AVPacket* packet;	//解码前的压缩数据（包数据）
	int index;
	unsigned char* out_buffer;	//数据缓冲区
	struct SwsContext* img_convert_ctx;
	frame_queue_t fq;

	int screen_w = 0, screen_h = 0;
	SDL_Window* screen; //SDL弹出的窗口
	SDL_Renderer* sdlRenderer; //渲染器，渲染SDL_Texture至SDL_Window
	SDL_Texture* sdlTexture; //纹理
	SDL_Rect sdlRect;
	SDL_Thread* video_tid; //多线程，同步刷新时间
	SDL_Event event; //线程状态

	int fps;
	//flag
	//int thread_exit = 0;
	//int thread_pause = 0;


};

//VideoPlayer::VideoPlayer(player_stat_t* input_is) {
//	this->is = input_is;
//}