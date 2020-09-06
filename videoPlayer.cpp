#include "VideoPlayer.h"
#include "main.h"

VideoPlayer::VideoPlayer(char* filepath, player_stat_t* is1) {
	is = is1;
	if (!(display_mutex = SDL_CreateMutex())) {
		printf("SDL_CreateMutex(): %s\n", SDL_GetError());
		return;
	}
	av_register_all();	//注册库
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();

	//初始化SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return;
	}

	//打开视频文件，初始化pFormatCtx
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return;
	}
	//获取文件信息
	if (avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf("Couldn't find stream information.\n");
		return;
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
		return;
	}
	//获取解码器
	pStream = pFormatCtx->streams[index];
	pCodecCtx = pFormatCtx->streams[index]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return;
	}
	//打开解码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
		printf("Could not open codec.\n");
		return;
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
	printf("---------------- File Information ---------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");

	//设置SDL界面
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow(filepath, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;
}


VideoPlayer::~VideoPlayer() {
	sws_freeContext(img_convert_ctx);
	av_free(out_buffer);
	av_free_packet(packet);
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	SDL_Quit();
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
}


void VideoPlayer::display_one_frame() {
	frame_t* pFrame_t = fq.peek_last();
	AVFrame* pFrame = pFrame_t->frame;
	//pFrameYUV = av_frame_alloc();
	//画面适配，适配后的像素数据存在pFrameYUV->data中
	sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
		pFrameYUV->data, pFrameYUV->linesize);
	//printf("=======================================\n");
	//SDL显示视频
	SDL_LockMutex(display_mutex);
	SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
	SDL_RenderPresent(sdlRenderer);
	SDL_UnlockMutex(display_mutex);
}


int VideoPlayer::video_refresh(double* remaining_time) {
	double time;
	bool flag = false;

	do {
		if (fq.nb_remaining() == 0) { //所有帧都已显示
			return 0;
		}

		double last_duration, duration, delay;
		frame_t* vp, * lastvp;

		lastvp = fq.peek_last();//上一张已显示的帧
		vp = fq.peek();         //待显示的帧

		last_duration = vp->pts - lastvp->pts;	//上一帧播放时长：待播放帧的播放时间与上一帧的时间差。通过调节此值来调节当前帧播放快慢
		delay = compute_target_delay(last_duration);

		time = av_gettime_relative()/1000000.0;
		//当前系统时刻 < 当前帧播放时刻，表示播放时刻未到，不播放，直接返回
		if (time < is->frame_timer + delay) {
			//更新刷新时间remaining_time为当前时刻到下一播放时刻的时间差，
			*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
			return 0;
		}

		//更新frame_timer值
		is->frame_timer += delay;
		//校正frame_timer值：若frame_timer落后于当前系统时间太久(超过最大同步域值)，则更新为当前系统时间
		if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX) {
			is->frame_timer = time;
		}

		SDL_LockMutex(fq.mutex);
		if (!isnan(vp->pts)) {
			set_clock(&is->video_clk, vp->pts);	//更新视频时钟
		}
		SDL_UnlockMutex(fq.mutex);

		//如果队列中只剩一帧，不丢弃没来得及播放的帧
		if (fq.nb_remaining() > 1) {
			frame_t* nextvp = fq.peek_next();  //下一个待显示的帧
			duration = nextvp->pts - vp->pts;  //当前帧vp播放时长
			//当前系统时刻 > 下一帧播放时刻，当前帧vp未能及时播放
			if (time > is->frame_timer + duration) {
				fq.pop();   //删除上一帧已显示帧(从lastvp更新到vp)
				flag = true;
			}
		}
	} while (flag);

	//删除当前读指针元素，读指针+1。若未丢帧，读指针从lastvp更新到vp；若有丢帧，读指针从vp更新到nextvp
	fq.pop();
	//播放
	display_one_frame();

	return 0;
}


// 根据视频时钟与音频时钟的差值，校正delay值，使视频时钟追赶（跳过帧）或等待（重复播放帧）音频时钟
double VideoPlayer::compute_target_delay(double delay) {
	// 若delay < AV_SYNC_THRESHOLD_MIN，同步域值为AV_SYNC_THRESHOLD_MIN
	// 若delay > AV_SYNC_THRESHOLD_MAX，同步域值为AV_SYNC_THRESHOLD_MAX
	// 若AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX，则同步域值为delay
	double sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
	//视频与音频时钟的差值，时钟值是上一帧pts值(实为：上一帧pts + 上一帧至今流逝的时间差)
	double diff = get_clock(&is->video_clk) - get_clock(&is->audio_clk);	

	//printf("video_clk=%.2f, audio_clk=%.2f, diff=%.2f\n", get_clock(&is->video_clk), get_clock(&is->audio_clk), diff);

	if (!isnan(diff)) {
		//视频时钟落后音频时钟，且超过同步域值
		if (diff <= -sync_threshold)        
			delay = FFMAX(0, delay + diff); // 当前帧播放时刻落后于音频时钟(delay+diff<0)则delay=0(视频追赶，立即播放)，否则delay=delay+diff
		//视频时钟超前音频时钟，且超过同步域值，放慢视频播放，加大延时
		else if (diff >= sync_threshold)
			delay = 2 * delay;
	}	

	return delay;
}


int VideoPlayer::video_play_thread() {
	double remaining_time = 0.0;

	while (!is->flag_exit) {
		if (is->flag_pause) continue;

		if (remaining_time > 0.0) {
			av_usleep((unsigned)(remaining_time * 1000000.0));
		}
		remaining_time = 1/refresh_rate;
		//判断延时时间并显示一帧
		video_refresh(&remaining_time);
	}

	return 0;
}


int VideoPlayer::decode_frame(AVFrame* pFrame) {
	int ret, got_picture;

	while (1) {
		while (1) {
			if (av_read_frame(pFormatCtx, packet) < 0) {//解封装媒体文件
				//is->flag_exit = 1;
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
			pFrame->pts = pFrame->best_effort_timestamp;
			return 1;
		}
		av_free_packet(packet);
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
	refresh_rate = frame_rate.num / frame_rate.den;
	//printf("refresh=%f\n", refresh_rate);

	if (p_frame == NULL) {
		printf("av_frame_alloc() for p_frame failed\n");
		return AVERROR(ENOMEM);
	}

	while (1) {
		got_picture = decode_frame(p_frame);
		if (got_picture < 0) {
			av_frame_free(&p_frame);
			return 0;
		}
		AVRational avr = {frame_rate.den, frame_rate.num};
		duration = (frame_rate.num && frame_rate.den ? av_q2d(avr) : 0);   // 当前帧播放时长
		pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb);   // 当前帧显示时间戳
		ret = fq.queue_picture(p_frame, pts, duration, p_frame->pkt_pos);   // 将当前帧压入frame_queue
		av_frame_unref(p_frame);

		if (ret < 0) {
			av_frame_free(&p_frame);
		}
	}

	return 0;
}


int VideoPlayer::render_refresh() {
	SDL_LockMutex(display_mutex);	//不加锁有时会冲突	
	SDL_RenderClear(sdlRenderer);
	SDL_RenderPresent(sdlRenderer);
	SDL_UnlockMutex(display_mutex);
	return 0;
}


int VideoPlayer::do_fullscreen() {
	if (is->flag_fullscreen) {
		SDL_SetWindowFullscreen(screen, SDL_TRUE);
	}
	else {
		SDL_SetWindowFullscreen(screen, SDL_FALSE);
	}
	render_refresh();
	return 0;
}


int VideoPlayer::resize_window(int width, int height) {
	//保持原始画面比例
	double tmp = double(screen_h) / screen_w * width;
	if (tmp < height) {
		sdlRect.x = 0;
		sdlRect.y = (height - tmp) / 2;
		sdlRect.w = width;
		sdlRect.h = tmp;
	}
	else {
		tmp = double(screen_w) / screen_h * height;
		sdlRect.x = (width - tmp) / 2;
		sdlRect.y = 0;
		sdlRect.w = tmp;
		sdlRect.h = height;
	}

	render_refresh();
	
	return 0;
}

void VideoPlayer::video_playing() {
	m_pPlay = std::move(std::make_shared<std::thread>(&VideoPlayer::video_play_thread, this));
	m_pDecode = std::move(std::make_shared<std::thread>(&VideoPlayer::video_decode_thread, this));
}