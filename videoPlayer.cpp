#include "VideoPlayer.h"
#include "main.h"
#include "VideoFrameQueue.h"

//创建了一个SDL线程，每隔固定时间（=刷新间隔）发送一个自定义的消息，告知主函数进行解码显示，使画面刷新间隔保持在40毫秒
int VideoPlayer::sfp_refresh_thread(void* opaque) {
	int fps = (int)opaque; //每秒多少帧，画面刷新间隔是(1000/frame_per_sec)毫秒，默认25
	//while (!thread_exit) {
	while (1) {
		SDL_Event event;
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(1000/fps);
	}
	//thread_exit = 0;

	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


void VideoPlayer::display_one_frame() {
	frame_t* pFrame_t = frame_queue_peek_last(&fq);
	AVFrame* pFrame = pFrame_t->frame;
	//pFrameYUV = av_frame_alloc();
	//画面适配，适配后的像素数据存在pFrameYUV->data中
	sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
		pFrameYUV->data, pFrameYUV->linesize);
	printf("11\n");
	//SDL显示视频
	SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
}

int VideoPlayer::video_player_init(char* filepath, int input_fps) {
	fps = input_fps;

	av_register_all();	//注册库
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//初始化SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
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
	pStream = pFormatCtx->streams[index];
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
	frame_queue_init(&fq, VIDEO_PICTURE_QUEUE_SIZE, 1);
	//缓冲区内存分配
	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	//缓冲区绑定到输出的AVFrame中
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
	//初始化img_convert_ctx
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


	//输出视频文件信息
	printf("---------------- File Information ---------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");

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

	m_pDecode1 = std::move(std::make_shared<std::thread>(&VideoPlayer::video_play_thread, this));
	m_pDecode2 = std::move(std::make_shared<std::thread>(&VideoPlayer::video_decode_thread, this));
	return 0;
}


int VideoPlayer::video_refresh(double* remaining_time) {
	double time;
	static bool first_frame = true;
	bool flag = false;

	do {
		if (frame_queue_nb_remaining(&fq) == 0) { // 所有帧已显示
			// nothing to do, no picture to display in the queue
			return 0;
		}

		double last_duration, duration, delay;
		frame_t* vp, * lastvp;

		/* dequeue the picture */
		lastvp = frame_queue_peek_last(&fq);     // 上一帧：上次已显示的帧
		vp = frame_queue_peek(&fq);              // 当前帧：当前待显示的帧

		// lastvp和vp不是同一播放序列(一个seek会开始一个新播放序列)，将frame_timer更新为当前时间
		if (first_frame) {
			is->frame_timer = av_gettime_relative() / 1000000.0;
			first_frame = false;
		}

		// 暂停处理：不停播放上一帧图像
		//if (is->paused)
		//	goto display;

		/* compute nominal last_duration */
		last_duration = vp_duration(lastvp, vp);        // 上一帧播放时长：vp->pts - lastvp->pts
		delay = compute_target_delay(last_duration);    // 根据视频时钟和同步时钟的差值，计算delay值

		time = av_gettime_relative()/1000000.0;
		// 当前帧播放时刻(is->frame_timer+delay)大于当前时刻(time)，表示播放时刻未到
		if (time < is->frame_timer + delay) {
			// 播放时刻未到，则更新刷新时间remaining_time为当前时刻到下一播放时刻的时间差
			*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
			// 播放时刻未到，则不播放，直接返回
			return 0;
		}

		// 更新frame_timer值
		is->frame_timer += delay;
		// 校正frame_timer值：若frame_timer落后于当前系统时间太久(超过最大同步域值)，则更新为当前系统时间
		if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) {
			is->frame_timer = time;
		}

		SDL_LockMutex(fq.mutex);
		if (!isnan(vp->pts)) {
			update_video_pts(is, vp->pts, vp->pos, vp->serial); // 更新视频时钟：时间戳、时钟时间
		}
		SDL_UnlockMutex(fq.mutex);

		// 是否要丢弃未能及时播放的视频帧
		if (frame_queue_nb_remaining(&fq) > 1) { // 队列中未显示帧数>1(只有一帧则不考虑丢帧)
			frame_t* nextvp = frame_queue_peek_next(&fq);  // 下一帧：下一待显示的帧
			duration = vp_duration(vp, nextvp);             // 当前帧vp播放时长 = nextvp->pts - vp->pts
			// 当前帧vp未能及时播放，即下一帧播放时刻(is->frame_timer+duration)小于当前系统时刻(time)
			if (time > is->frame_timer + duration) {
				frame_queue_next(&fq);   // 删除上一帧已显示帧，即删除lastvp，读指针加1(从lastvp更新到vp)
				flag = true;
			}
		}
	} while (flag);

	// 删除当前读指针元素，读指针+1。若未丢帧，读指针从lastvp更新到vp；若有丢帧，读指针从vp更新到nextvp
	frame_queue_next(&fq);

//display:
	display_one_frame();                      // 取出当前帧vp(若有丢帧是nextvp)进行播放
	return 0;
}


double VideoPlayer::vp_duration(frame_t* vp, frame_t* nextvp) {
	if (vp->serial == nextvp->serial) {
		double duration = nextvp->pts - vp->pts;
		if (isnan(duration) || duration <= 0)
			return vp->duration;
		else
			return duration;
	}
	else {
		return 0.0;
	}
}


// 根据视频时钟与同步时钟(如音频时钟)的差值，校正delay值，使视频时钟追赶或等待同步时钟
// 输入参数delay是上一帧播放时长，即上一帧播放后应延时多长时间后再播放当前帧，通过调节此值来调节当前帧播放快慢
// 返回值delay是将输入参数delay经校正后得到的值
double VideoPlayer::compute_target_delay(double delay) {
	double sync_threshold, diff = 0;

	/* update delay to follow master synchronisation source */

	/* if video is slave, we try to correct big delays by
	   duplicating or deleting a frame */
	   // 视频时钟与同步时钟(如音频时钟)的差异，时钟值是上一帧pts值(实为：上一帧pts + 上一帧至今流逝的时间差)
	diff = get_clock(&is->video_clk) - get_clock(&is->audio_clk);
	// delay是上一帧播放时长：当前帧(待播放的帧)播放时间与上一帧播放时间差理论值
	// diff是视频时钟与同步时钟的差值

	/* skip or repeat frame. We take into account the
	   delay to compute the threshold. I still don't know
	   if it is the best guess */
	   // 若delay < AV_SYNC_THRESHOLD_MIN，则同步域值为AV_SYNC_THRESHOLD_MIN
	   // 若delay > AV_SYNC_THRESHOLD_MAX，则同步域值为AV_SYNC_THRESHOLD_MAX
	   // 若AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX，则同步域值为delay
	sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
	if (!isnan(diff)) {
		if (diff <= -sync_threshold)        // 视频时钟落后于同步时钟，且超过同步域值
			delay = FFMAX(0, delay + diff); // 当前帧播放时刻落后于同步时钟(delay+diff<0)则delay=0(视频追赶，立即播放)，否则delay=delay+diff
		else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)  // 视频时钟超前于同步时钟，且超过同步域值，但上一帧播放时长超长
			delay = delay + diff;           // 仅仅校正为delay=delay+diff，主要是AV_SYNC_FRAMEDUP_THRESHOLD参数的作用
		else if (diff >= sync_threshold)    // 视频时钟超前于同步时钟，且超过同步域值
			delay = 2 * delay;              // 视频播放要放慢脚步，delay扩大至2倍
	}
	
	printf("video: delay=%0.3f A-V=%f\n", delay, -diff);

	return delay;
}


void VideoPlayer::update_video_pts(player_stat_t* is, double pts, int64_t pos, int serial) {
	/* update current video pts */
	set_clock(&is->video_clk, pts);            // 更新vidclock
	//-sync_clock_to_slave(&is->extclk, &is->vidclk);  // 将extclock同步到vidclock
}


int VideoPlayer::video_play_thread() {
	double remaining_time = 0.0;

	while (1) {
		if (remaining_time > 0.0) {
			av_usleep((unsigned)(remaining_time * 1000000.0));
		}
		remaining_time = 1/fps;
		// 立即显示当前帧，或延时remaining_time后再显示
		video_refresh(&remaining_time);
	}

	return 0;
}

int VideoPlayer::decode_frame(AVFrame* pFrame) {

	int ret, got_picture;
	//video_tid = SDL_CreateThread(sfp_refresh_thread, NULL, (void*)fps);

	while (1) {
		//SDL_WaitEvent(&event);
		//if (event.type==SFM_REFRESH_EVENT) {	//经过40ms刷新一次
			while (1) {
				if (av_read_frame(pFormatCtx, packet) < 0) {//解封装媒体文件
					//thread_exit = 1;
					//printf("av_read_frame() error.\n");
					return 0;
				}
				if (packet->stream_index == index) {
					break;
				}
			}

			// 解码packet至pFrame		
			ret = avcodec_send_packet(pCodecCtx, packet);	//将AVPacket数据放入待解码队列中
			if (ret < 0) {
				printf("Decode Error.\n");
			}		
			got_picture = avcodec_receive_frame(pCodecCtx, pFrame);	//从解码完成队列中取出一个AVFrame数据
			if (got_picture==0) {					
				//display_one_frame(pFrame);
				pFrame->pts = pFrame->best_effort_timestamp;
			}
			av_free_packet(packet);
		//}
		//else if (event.type==SDL_QUIT) {
		//	//thread_exit = 1;
		//	break;
		//}
		//else if (event.type==SFM_BREAK_EVENT) {
		//	break;
		//}
	}
	
	return 0;
}


int VideoPlayer::video_decode_thread() {
	AVFrame* p_frame = av_frame_alloc();
	double pts;
	double duration;
	int ret, got_picture;
	AVRational tb = pStream->time_base;
	AVRational frame_rate = av_guess_frame_rate(pFormatCtx, pStream, NULL);

	if (p_frame == NULL) {
		printf("av_frame_alloc() for p_frame failed\n");
		return AVERROR(ENOMEM);
	}

	while (1) {
		got_picture = decode_frame(p_frame);
		if (got_picture < 0) {
			av_frame_free(&p_frame);
		}
		AVRational avr = {frame_rate.den, frame_rate.num};
		duration = (frame_rate.num && frame_rate.den ? av_q2d(avr) : 0);   // 当前帧播放时长
		pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb);   // 当前帧显示时间戳
		ret = queue_picture(&fq, p_frame, pts, duration, p_frame->pkt_pos);   // 将当前帧压入frame_queue
		av_frame_unref(p_frame);

		if (ret < 0) {
			av_frame_free(&p_frame);
		}
	}

	return 0;
}


int VideoPlayer::destroy() {
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
