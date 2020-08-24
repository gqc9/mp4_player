#pragma once

#pragma once

#include <stdio.h>
#include <cstdlib>
#include <queue>
#include "al.h"
#include "alc.h"

#include "main.h"

extern "C" {
#include "libavformat/avformat.h"	//封装格式
#include "libavcodec/avcodec.h"	//解码
#include "libswresample/swresample.h"
};


#define MAX_AUDIO_FARME_SIZE 48000 * 2
#define NUMBUFFERS (4)


typedef struct _tFrame {
    void* data;
    int size;
    int samplerate;
}TFRAME, * PTFRAME;


class AudioPlayer {
public:
    int audio_player_init(char* filepath);
    int decode();
    int audio_playing();

public:
    player_stat_t* is;

private:
    int OpenAL_init();
    int SoundCallback(ALuint& bufferID);
    int Play();
    int destory();
    //int start_playing();
    int audio_play_thread();

private:
    std::shared_ptr<std::thread> m_pAudio;

    AVFormatContext* pFormatCtx; //解封装
    AVCodecContext* pCodecCtx; //解码
    AVCodec* pCodec;
    AVFrame* pFrame, * pFrameYUV; //帧数据
    AVPacket* packet;	//解码前的压缩数据（包数据）
    int index; //编码器索引位置
    uint8_t* out_buffer;	//数据缓冲区
    int out_buffer_size;    //缓冲区大小
    SwrContext* swrCtx;

    enum AVSampleFormat out_sample_fmt;
    int out_sample_rate;
    int out_channel_nb;


    std::queue<PTFRAME> queueData; //保存解码后数据
    ALuint m_source;
    ALuint m_buffers[NUMBUFFERS];
};


