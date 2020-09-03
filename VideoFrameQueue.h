#pragma once

#include "main.h"

class VideoFrameQueue {
public:
	VideoFrameQueue(int max_size, int keep_last);
	~VideoFrameQueue();
	frame_t* peek();
	frame_t* peek_next();
	frame_t* peek_last();
	frame_t* peek_writable();
	frame_t* peek_readable();
	void push();
	void pop();
	int nb_remaining();
	int queue_picture(AVFrame* src_frame, double pts, double duration, int64_t pos);

public:
	SDL_mutex* mutex;
	SDL_cond* cond;


private:
	frame_t queue[FRAME_QUEUE_SIZE];
	int rindex = 0;                     // 读索引。待播放时读取此帧进行播放，播放后此帧成为上一帧
	int windex = 0;                     // 写索引
	int size = 0;                       // 总帧数
	int max_size;                   // 队列可存储最大帧数
	int keep_last;
	int rindex_shown = 0;               // 当前是否有帧在显示


};

