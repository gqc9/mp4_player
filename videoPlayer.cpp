#include <stdio.h>

extern "C" {
#include "libavformat/avformat.h"	//封装格式
#include "libavcodec/avcodec.h"	//解码
#include "libswscale/swscale.h"	//缩放
#include "libavutil/imgutils.h" //播放图像使用
#include "SDL2/SDL.h"	//播放
};


#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)
#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)


//flag
int thread_exit = 0;
//int thread_pause = 0;

//创建了一个SDL线程，每隔固定时间（=刷新间隔）发送一个自定义的消息，告知主函数进行解码显示，使画面刷新间隔保持在40毫秒
int sfp_refresh_thread(void* opaque) {
	int fps = (int)opaque; //每秒多少帧，画面刷新间隔是(1000/frame_per_sec)毫秒，默认25
	while (!thread_exit) {
		SDL_Event event;
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(1000/fps);
	}
	thread_exit = 0;

	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


int playVideo(char* filepath, int fps) {
	//获取文件路径
	//char filepath[] = "test.mp4";		

	AVFormatContext* pFormatCtx; //解封装
	AVCodecContext* pCodecCtx; //解码
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV; //帧数据
	AVPacket* packet;	//解码前的压缩数据（包数据）
	int index;
	unsigned char* out_buffer;	//数据缓冲区
	struct SwsContext* img_convert_ctx;

	int screen_w = 0, screen_h = 0;
	SDL_Window* screen; //SDL弹出的窗口
	SDL_Renderer* sdlRenderer; //渲染器，渲染SDL_Texture至SDL_Window
	SDL_Texture* sdlTexture; //纹理
	SDL_Rect sdlRect;
	SDL_Thread* video_tid; //多线程，同步刷新时间
	SDL_Event event; //线程状态

	av_register_all();	//注册库
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//初始化SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	//打开视频文件，初始化pFormatCtx
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	//获取文件信息
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	//获取各个媒体流的编码器信息，找到对应的type所在的pFormatCtx->streams的索引位置，初始化编码器
	index = -1;
	for (int i = 0; i<pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			index = i;
			break;
		}
	if (index == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}
	//获取解码器
	pCodecCtx = pFormatCtx->streams[index]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	//打开解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		printf("Could not open codec.\n");
		return -1;
	}

	//内存分配
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	//缓冲区内存分配
	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	//缓冲区绑定到输出的AVFrame中
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
	//初始化img_convert_ctx
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


	//输出视频文件信息
	//printf("---------------- File Information ---------------\n");
	//av_dump_format(pFormatCtx, 0, filepath, 0);
	//printf("-------------------------------------------------\n");

	//设置SDL界面
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow(filepath, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	int ret, got_picture;
	video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, (void*)fps);

	while (1) {
		SDL_WaitEvent(&event);
		if (event.type==SFM_REFRESH_EVENT) {	//经过40ms刷新一次
			while (1) {
				if (av_read_frame(pFormatCtx, packet) < 0) //解封装媒体文件
					thread_exit = 1;
				if (packet->stream_index == index)
					break;
			}
			// 解码packet至pFrame
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);	
			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			if (got_picture) {
				//画面适配，适配后的像素数据存在pFrameYUV->data中
				sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize);
				//SDL显示视频
				SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
				SDL_RenderClear(sdlRenderer);
				SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
				SDL_RenderPresent(sdlRenderer);
			}
			av_free_packet(packet);
		}
		else if (event.type==SDL_QUIT) {
			thread_exit = 1;
		}
		else if (event.type==SFM_BREAK_EVENT) {
			break;
		}
	}
	sws_freeContext(img_convert_ctx);
	av_free(out_buffer);
	av_free_packet(packet);
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	SDL_Quit();
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	return 0;
}
