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
//#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))
#define FRAME_QUEUE_SIZE SUBPICTURE_QUEUE_SIZE


typedef struct {
    AVFrame* frame;
    int serial;
    double pts;           /* presentation timestamp for the frame */
    double duration;      /* estimated duration of the frame */
    int64_t pos;                    // frame对应的packet在输入文件中的地址偏移
    int width;
    int height;
    int format;
    AVRational sar;
    int uploaded;
    int flip_v;
}   frame_t;


typedef struct {
    frame_t queue[FRAME_QUEUE_SIZE];
    int rindex;                     // 读索引。待播放时读取此帧进行播放，播放后此帧成为上一帧
    int windex;                     // 写索引
    int size;                       // 总帧数
    int max_size;                   // 队列可存储最大帧数
    int keep_last;
    int rindex_shown;               // 当前是否有帧在显示
    SDL_mutex* mutex;
    SDL_cond* cond;
    //packet_queue_t* pktq;           // 指向对应的packet_queue
}   frame_queue_t;


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

    //packet_queue_t audio_pkt_queue;
    //packet_queue_t video_pkt_queue;
    //frame_queue_t audio_frm_queue;
    //frame_queue_t video_frm_queue;

    //struct SwsContext* img_convert_ctx;
    //struct SwrContext* audio_swr_ctx;
    //AVFrame* p_frm_yuv;

    //audio_param_t audio_param_src;
    //audio_param_t audio_param_tgt;
    //int audio_hw_buf_size;              // SDL音频缓冲区大小(单位字节)
    //uint8_t* p_audio_frm;               // 指向待播放的一帧音频数据，指向的数据区将被拷入SDL音频缓冲区。若经过重采样则指向audio_frm_rwr，否则指向frame中的音频
    //uint8_t* audio_frm_rwr;             // 音频重采样的输出缓冲区
    //unsigned int audio_frm_size;        // 待播放的一帧音频数据(audio_buf指向)的大小
    //unsigned int audio_frm_rwr_size;    // 申请到的音频缓冲区audio_frm_rwr的实际尺寸
    //int audio_cp_index;                 // 当前音频帧中已拷入SDL音频缓冲区的位置索引(指向第一个待拷贝字节)
    //int audio_write_buf_size;           // 当前音频帧中尚未拷入SDL音频缓冲区的数据量，audio_frm_size = audio_cp_index + audio_write_buf_size
    double audio_clock;
    //int audio_clock_serial;

    //int abort_request;
    //int paused;
    //int step;

    //SDL_cond* continue_read_thread;
    //SDL_Thread* read_tid;           // demux解复用线程

}   player_stat_t;



double get_clock(play_clock_t* c);
void set_clock_at(play_clock_t* c, double pts, /*int serial, */double time);
void set_clock(play_clock_t* c, double pts);