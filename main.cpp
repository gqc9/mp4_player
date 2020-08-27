#include "main.h"

#include "VideoPlayer.h"
#include "VideoFrameQueue.h"
#include "AudioPlayer.h"



// 返回值：返回上一帧的pts更新值(上一帧pts+流逝的时间)
double get_clock(play_clock_t* c) {
    //if (*c->queue_serial != c->serial)
    //{
    //    return NAN;
    //}
    if (c->paused)
    {
        return c->pts;
    }
    else
    {
        double time = av_gettime_relative() / 1000000.0;
        double ret = c->pts_drift + time;   // 展开得： c->pts + (time - c->last_updated)
        return ret;
    }
}

void set_clock_at(play_clock_t* c, double pts, /*int serial, */double time)
{
    c->pts = pts;
    c->last_updated = time;
    c->pts_drift = c->pts - time;
    //c->serial = serial;
}

void set_clock(play_clock_t* c, double pts)
{
    double time = av_gettime_relative() / 1000000.0;
    set_clock_at(c, pts, time);
}

static void set_clock_speed(play_clock_t* c, double speed)
{
    set_clock(c, get_clock(c));
    c->speed = speed;
}

void init_clock(play_clock_t* c)
{
    c->speed = 1.0;
    c->paused = 0;
    set_clock(c, NAN);
}

static void sync_play_clock_to_slave(play_clock_t* c, play_clock_t* slave)
{
    double clock = get_clock(c);
    double slave_clock = get_clock(slave);
    if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
        set_clock(c, slave_clock);
}



int main(int argc, char* argv[]) {
	//获取文件路径
	char filepath[] = "F:/bupt/网研保研/player/Debug/jojo.mp4";		

	int fps = 25; // 每秒多少帧，用于确定画面刷新间隔，默认值25
	/*char* filepath;
	if (argc==2) {
		filepath = argv[1];
	}
	else if (argc==3) {
		filepath = argv[1];
		fps = atoi(argv[2]);
	}
	else {
		printf("Usage: 'player <filename>' or 'player <filename> <fps>'");
		return -1;
	}*/

	player_stat_t* is;
    is = (player_stat_t*)av_mallocz(sizeof(player_stat_t));
    if (!is) {
        return -1;
    }
    init_clock(&is->video_clk);
    init_clock(&is->audio_clk);

	AudioPlayer* audioPlayer = new AudioPlayer;
    audioPlayer->is = is;
    audioPlayer->audio_player_init(filepath);
    
    VideoPlayer* videoPlayer = new VideoPlayer;
    videoPlayer->is = is;
	videoPlayer->video_player_init(filepath, fps);

    audioPlayer->decode();
    videoPlayer->video_playing();
    audioPlayer->audio_playing();



    system("pause");

    return 0;
}