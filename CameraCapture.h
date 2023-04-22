#pragma once
#include "ReadParams.h"
#include "DataPacket.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavdevice/avdevice.h>
#include <libavutil/dict.h>
}

class CameraCapture
{
public:
    // constructor, params from class ReadParams
    CameraCapture(const CameraParams* params = nullptr);

    // destructor for libav pointers
    ~CameraCapture() {
        av_dict_free(&CamInDict);
        avcodec_free_context(&CodecCtx);
        avformat_close_input(&Input_fmt_Ctx);
        if (FrameOut) {
            av_frame_free(&FrameOut);
        }
        sws_freeContext(SwsCtx);
        SwsCtx = nullptr;
    };

    void set_cam_params(const CameraParams* params) {
        CamParams = params;
    }

    void set_callback_func(std::function<void(DataPacket*)> func) {
        OnFrameFunc = func;
    }

    bool ready() const {
        return (State == 1) && (OnFrameFunc != nullptr);
    }

    int initialize();

    void capture();

private:
    std::function<void(DataPacket*)> OnFrameFunc;

    AVFormatContext*    Input_fmt_Ctx;
    std::clock_t        DelayClock;
    AVCodecContext*     CodecCtx;
    AVDictionary*       CamInDict;
    SwsContext*         SwsCtx;
    AVFrame*            FrameOut;
    DataPacket          DataOut;
    uint64_t            FrameCnt;
    uint64_t            Bitrate;
    int                 DataOutSz;
    int                 OutStreamNum;
    int                 State; // capture ready state
    const CameraParams* CamParams;
};
