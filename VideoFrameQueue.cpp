#include "VideoFrameQueue.h"
#include "main.h"


VideoFrameQueue::VideoFrameQueue() {
    memset(queue, 0, sizeof(queue));
    if (!(mutex = SDL_CreateMutex())) {
        printf("SDL_CreateMutex(): %s\n", SDL_GetError());
        return;
    }
    if (!(cond = SDL_CreateCond())) {
        printf("SDL_CreateCond(): %s\n", SDL_GetError());
        return;
    }
    max_size = FRAME_QUEUE_SIZE;
    for (int i = 0; i < max_size; i++) {
        if (!(queue[i].frame = av_frame_alloc())) {
            printf("alloc() error\n");
            return;
        }
    }
}

VideoFrameQueue::~VideoFrameQueue() {
    for (int i = 0; i < max_size; i++) {
        frame_t* vp = &queue[i];
        av_frame_unref(vp->frame);
        av_frame_free(&vp->frame);
    }
    SDL_DestroyMutex(mutex);
    SDL_DestroyCond(cond);
}

//当前帧
frame_t* VideoFrameQueue::peek() {
    return &queue[(rindex + rindex_shown) % max_size];
}

//下一帧
frame_t* VideoFrameQueue::peek_next() {
    return &queue[(rindex + rindex_shown + 1) % max_size];
}

//上一帧
frame_t* VideoFrameQueue::peek_last() {
    return &queue[rindex];
}

//向队列尾部申请一个可写的帧空间，若无空间可写，则等待
frame_t* VideoFrameQueue::peek_writable() {
    SDL_LockMutex(mutex);
    while (size >= max_size) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    return &queue[windex];
}

//只读取不删除，若无帧可读则等待
frame_t* VideoFrameQueue::peek_readable() {
    SDL_LockMutex(mutex);
    while (size - rindex_shown <= 0) {
        SDL_CondWait(cond, mutex);
    }
    SDL_UnlockMutex(mutex);

    return &queue[(rindex + rindex_shown) % max_size];
}

//压入一帧，只更新计数与写指针
void VideoFrameQueue::push() {
    if (++windex == max_size)
        windex = 0;
    SDL_LockMutex(mutex);
    size++;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

//删除一帧，读指针加1
void VideoFrameQueue::pop() {
    if (!rindex_shown) {
        rindex_shown = 1;
        return;
    }
    av_frame_unref(queue[rindex].frame);
    if (++rindex == max_size)
        rindex = 0;
    SDL_LockMutex(mutex);
    size--;
    SDL_CondSignal(cond);
    SDL_UnlockMutex(mutex);
}

//返回队列中未显示的帧数
int VideoFrameQueue::nb_remaining() {
    return size - rindex_shown;
}

//向队列中压入一帧，同时拷入帧数据
int VideoFrameQueue::queue_picture(AVFrame* src_frame, double pts, double duration, int64_t pos) {
    //分配空间
    frame_t* vp = peek_writable();
    if (!vp)
        return -1;

    vp->pts = pts;
    vp->duration = duration;

    // 将AVFrame拷入队列相应位置
    av_frame_move_ref(vp->frame, src_frame);

    // 更新队列计数及写索引
    push();
    return 0;
}