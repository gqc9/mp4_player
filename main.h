#pragma once

#include <stdio.h>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"

extern "C" {
#include "libavformat/avformat.h"	//封装格式
#include "libavcodec/avcodec.h"	//解码
#include "libswscale/swscale.h"	//缩放
#include "libswresample/swresample.h" //重采样
#include "libavutil/imgutils.h" //播放图像使用
#include "libavutil/time.h"
#include "SDL2/SDL.h"	//播放
};


//同步阈值的最小值，如果小于它、不会校正
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
//同步阈值的最大值
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
//视频帧队列的大小
#define FRAME_QUEUE_SIZE 16


typedef struct {
    AVFrame* frame;
    double pts;  //presentation timestamp
    double duration;
} frame_t;


typedef struct {
    double pts;         // 当前帧(待播放)显示时间戳，播放后，当前帧变成上一帧
    double pts_drift;   // 当前帧显示时间戳与当前系统时钟时间的差值
    double last_updated;// 当前时钟(如视频时钟)最后一次更新时间，也可称当前时钟时间
} play_clock_t;


typedef struct { 
    play_clock_t audio_clk; // 音频时钟
    play_clock_t video_clk; // 视频时钟
    double frame_timer;
    //flag
    int flag_exit;
    int flag_pause;
    int flag_fullscreen;
    int flag_resize;
    int forward;
}   player_stat_t;


double get_clock(play_clock_t* c);
void set_clock(play_clock_t* c, double pts);