# Player

有很多功能的mp4格式视频文件播放器。

## Usage

编译后，进入Debug目录下运行cmd：

`player <filename>`

* `filename`：将需要播放的视频文件放到`player.exe`同一目录下，直接输入文件名；或使用文件完整路径。

* 若提示找不到dll文件，将相应dll文件放到`player.exe`同一目录下即可

* 按`p`暂停，再按`p`继续播放
* 按`1`前进10秒，`3`前进30秒
* 按`u`增大音量，`d`减小音量
* 可以调整窗口大小，画面保持比例缩放
* 按`f`全屏播放，再按`f`取消全屏
* 按`q`加速播放，按一次加速0.5倍，最多加速至2倍
* 按`s`减速播放，按一次减速0.5倍，最多减速至0.5倍（调节范围：0.5倍->1倍->1.5倍->2倍）

## Dependency

* [FFMPEG](http://ffmpeg.org/)

* [SDL2.0](https://www.libsdl.org/download-2.0.php)

* [OpenAL](http://www.openal.org/)
