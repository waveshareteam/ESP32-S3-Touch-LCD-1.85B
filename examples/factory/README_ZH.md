# 出厂应用

[English](README.md) | [简体中文](README_ZH.md)

## 功能

提供适配 360 × 360 圆屏的桌面，每页四个图标：Display Test、Settings、AIChats、Battery、Touch Test、Photos、Weather、Music、Sensor、2048、Mic Test、Button Test、Video、Gravity、Badge、Lucky Wheel 和 Watch。左右滑动切换页面，打开 AIChats 即可使用语音对话。

## 硬件需求

ESP32-S3-Touch-LCD-1.85B、扬声器及配套 3.7V、500mAh 电池；联网功能需要 2.4GHz Wi-Fi。媒体功能需要 FAT32 microSD 卡，JPEG 图片放入 `photo/`，MP3 音频放入 `music/`，AVI/MJPEG 视频放入 `video/`（帧尺寸不大于 360 × 360，视频不播放音轨）。

## 怎么运行

直接体验请按[固件说明](../../firmware/README_ZH.md)下载并烧录。自行编译请使用 ESP-IDF v5.5.4，在本目录执行：

```sh
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

在 Settings 中打开手机连接入口，扫描 Wi-Fi 二维码加入开发板热点；也可手动连接 `S3-LCD-1.85B-xxxxxx`（密码 `12345678`）。打开 `http://192.168.4.1`，首页选择 **Wi-Fi Setup** 配网或 **Transfer Images** 传图。部分手机连接热点后会自动弹出网页。设备联网后打开 AIChats，如出现激活提示，按界面说明完成操作。

## 预期现象

桌面显示时间、Wi-Fi 和电池状态；相册铺满圆屏，视频预览区域为 240 × 240。Mic Test 分别显示 MIC1/MIC2 音量条，可录制最多 5 秒并单独回放；AIChats 支持语音对话、唤醒词打断，以及语音调节音量和亮度。

## 常见问题

### 一个热点，两个网页功能

`192.168.4.1` 始终显示设备管理首页。选择 **Wi-Fi Setup** 进入 `/wifi`，
可扫描附近的 2.4GHz 网络、选择网络（或手动输入隐藏 SSID）并保存连接信息。
选择 **Transfer Images** 进入 `/badge`，网页直接启动图片管理，无需先在设备上打开 Badge。
两个功能页均提供返回首页的入口。

设备二维码用于加入热点，保留原来的首页网址，不需要另外扫描网页二维码。
手机提示“无互联网”时请保持连接。配网成功后热点继续保留，方便随后传图；
退出设备上的页面也不会中断由手机网页持有的编辑会话。传图完成后点击网页 **Done**
返回首页，再切换配网。首页的 **Disconnect & close hotspot** 可关闭热点；
网页连续十分钟没有请求时会释放会话，需要时重新进入设备手机连接页面开启热点。
两个网页共用同一个 HTTP 服务；图片编辑期间暂停接受配网和扫描请求，避免影响传图。

### 电子徽章

Badge 的初始引导页、控制菜单、手机连接提示和网页图片管理统一使用暖白与浅绿色。
打开 **Badge** 全屏显示图片。尚未添加图片时，点击引导页的 **Add images**；
也可以短按图片后选择 **Manage images**。手机连接屏幕显示的热点
（密码 `12345678`），打开 `http://192.168.4.1` 并选择 **Transfer Images**。网页提供六个图片槽位，可选择
JPEG、PNG 或 WebP，拖动、缩放圆形预览后上传；浏览器会生成 360 × 360 的 JPEG。
网页还支持切换当前图片和删除图片。点击网页 **Done** 结束编辑并返回设备管理首页。
设备上的 **Done** 返回图片显示，手机已打开的编辑页仍可继续使用；也可从 Settings 的热点直接进入传图。
图片保持显示，不自动轮播。左右滑动切换有图片的槽位，到末尾后循环；只有一张图片时不会变化。
轻点图片打开暖白色全屏控制菜单，显示当前图片的圆形预览、图片数量和分页圆点。通过左右箭头切图，选择 **Manage images**
重新添加或替换图片，选择 **Exit** 退出 APP。选择 **Close** 收起菜单；全屏显示图片时，
也可以从底部边缘向上滑动退出 APP。
长按屏幕以及长按后松手均不会打开控制菜单。

图片及当前选择保存在设备内部 Flash，正常重启后保留，无需 SD 卡和互联网。
通过网页上传无需提前打开设备上的 Badge，上传后再打开 Badge 即可查看。
徽章使用现有的 4MB `assets` SPIFFS 分区，挂载路径为 `/badge`，没有有效文件系统时
会在首次使用时格式化；应用和模型分区地址不变。整片擦除或覆盖 `assets` 会删除徽章图片。
单张上传上限 256KB，图片验证通过后才替换原图。

网页编辑器参考 [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_badge)，
MIT 许可声明保留在 `components/Badge/LICENSE-M5Stack`。
桌面图标采用产品银色圆形外壳，屏幕内显示绘制的照片图案。可运行
`python components/Badge/assets/generate_icon.py` 重新生成
（需要 Pillow 和随附的微雪产品外观素材 `badge-device-shell.png`）。

### 幸运转盘

打开 **Lucky Wheel**，短按中央 **SPIN** 开始。浅彩转盘贴边铺满圆屏，没有外圈留白和装饰边框，随机选择旋转方向，
在 2.4～3.6 秒内逐渐减速，中央显示顶部固定指针对应的结果。再次轻点中央即可重转。
每个选项等概率，转动过程中忽略重复点击；数字随扇区移动并保持正向，方便阅读。

点击 **Options**，可设置 2～18 个编号选项（默认 8 个），点击 **Done** 生效。
选项数量保留到重启，不写入 Flash。选择 **Exit to desktop**、从转盘底部向上滑动，
或按 BOOT 可退出；无需网络和 SD 卡。

转盘行为参考 [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_lucky_wheel)，
MIT 许可声明保留在 `components/LuckyWheel/LICENSE-M5Stack`，界面与桌面图标为本工程绘制。
可运行 `python components/LuckyWheel/assets/generate_icon.py` 重新生成 112 × 112 图标
（需要 Pillow）。

### 时钟表盘

打开 **Watch**，显示与桌面图标一致的象牙白模拟表盘、十二道灰色刻度、深色时分针和红色扫秒针。
轻点屏幕依次切换纯净表盘、时间日期信息、仅指针三种视图。从底部边缘上滑或按 BOOT 退出，
无需 SD 卡。

开机后由网络后台读取硬件 RTC，为系统时钟设置初值；Wi-Fi 联网后通过 SNTP 校准为北京时间
（UTC+8），再由后台把本地日历写回 RTC，供下次离线启动使用。表盘与桌面读取同一系统时钟，
Sensor 显示硬件日历。断网后时钟继续走时。信息视图在网络校时前显示 **RTC**，校时后显示
**SYNCED**，之后断网也保留校时状态。两个时间来源均不可用时显示 **Waiting for time**，
不显示虚构时间。RTC 复位或失去备份供电后可能保留默认日期，需要联网校时一次；离线精度
取决于 RTC 供电保持和晶振漂移。时区固定为 UTC+8。

经典表盘交互参考 [M5Stack M5StopWatch-UserDemo](https://github.com/m5stack/M5StopWatch-UserDemo/tree/81099c784991dc77f522de1a5bf62ec5de9ae0b5/main/apps/app_watch_face)，
MIT 许可声明保留在 `components/Watch/LICENSE-M5Stack`。表盘使用原生绘图，图标为本工程绘制；
可运行 `python components/Watch/assets/generate_icon.py` 重新生成图标（需要 Pillow）。

### 其他应用

电量测试请先接电池再接 USB，仅 USB 供电或运行中插拔电池时读数可能不准确，电量计温度不代表环境温度；录音为临时保存，使用另一个音频应用前请先退出当前应用。
