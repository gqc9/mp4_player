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


/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))


typedef struct {
    AVFrame* frame;
    //int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    //int64_t pos;                    // frame对应的packet在输入文件中的地址偏移
    //int width;
    //int height;
    //int format;
    //AVRational sar;
    //int uploaded;
    //int flip_v;
}   frame_t;


typedef struct {
    double pts;                     // 当前帧(待播放)显示时间戳，播放后，当前帧变成上一帧
    double pts_drift;               // 当前帧显示时间戳与当前系统时钟时间的差值
    double last_updated;            // 当前时钟(如视频时钟)最后一次更新时间，也可称当前时钟时间
    double speed;                   // 时钟速度控制，用于控制播放速度
    //int serial;                     // 播放序列，所谓播放序列就是一段连续的播放动作，一个seek操作会启动一段新的播放序列
    int paused;                     // 暂停标志
    //int* queue_serial;              // 指向packet_serial
}   play_clock_t;


typedef struct { 
    play_clock_t audio_clk;                   // 音频时钟
    play_clock_t video_clk;                   // 视频时钟
    double frame_timer;
    double audio_clock;
    //flag
    int flag_exit;
    int flag_pause;
    int forward;
}   player_stat_t;



double get_clock(play_clock_t* c);
void set_clock(play_clock_t* c, double pts);