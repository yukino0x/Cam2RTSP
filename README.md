# Cam2RTSP - H265 RTSP Server, Streaming from PC Camera

Open Camera with `dshow`, read data from camera with ffmpeg, encode the data with libx265, then send it to live555 RTSP Sever for streaming.

### Builds

All 3rd libraries are built, and included in `lib` and `include` folder.

* `ffmpeg: 4.3`
* `live555: 2023.03.20`
* `openssl`

### Usage

1. Modify the `config.txt`, change the `camera_name` to your own camera name, you can also change other configs such as framerate or resolution, its due to your camera and device performance.
2. Open `Cam2RTSP.sln` with `Visual Studio`(My VS version is 2022), run the project in `Debug x64`, you can also run in `Realse x64`, but you need to reconfigure the project properties. (I did not try `x86`)
3. Find the RTSP url that start with `rtsp://` in the output message, then play the url in vlc or other mainstream player.

### Reference

[jiaxin-du/RTSPMultiCam: A RTSP server for streaming combined picture from multiple cameras based on FFMPEG and live555 (github.com)](https://github.com/jiaxin-du/RTSPMultiCam)

