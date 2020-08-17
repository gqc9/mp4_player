# Vedio Player

播放mp4格式的视频文件（仅画面）

## Usage

编译后，进入Debug目录下运行cmd：

`player <filename>` or `player <filename> <fps>`

* filename：将需要播放的视频文件放到Debug目录下，直接输入文件名；或使用文件完整路径。

* fps：视频的帧数。不输入时默认为25帧/秒。

* 若提示找不到dll文件，将相应dll文件放到`player.exe`同一目录下即可

## Reference

[雷霄骅](http://blog.csdn.net/leixiaohua1020)的[最简单的基于FFMPEG+SDL的视频播放器](https://blog.csdn.net/leixiaohua1020/article/details/38868499)

[FFMPEG官方示例](https://github.com/FFmpeg/FFmpeg/tree/master/doc/examples)