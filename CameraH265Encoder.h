#pragma once
#include "DataPacket.h"
#include "ReadParams.h"
#include <queue>


extern "C" {
#include <libavutil\opt.h>
#include <libavutil\mathematics.h>
#include <libavformat\avformat.h>
#include <libswscale\swscale.h>
#include <libswresample\swresample.h>
#include <libavutil\imgutils.h>
#include <libavcodec\avcodec.h>
}


class CameraH265Encoder
{
public:
    static const char* codec;
   public:
       // CameraH265Encoder(int ncol = 2, int nrow = 2);
       CameraH265Encoder();
       ~CameraH265Encoder() {
           finalise();
       };

       void finalise();

       void set_callback_func(std::function<void()> func)
       {
           OnFrameFunc = func;
       };

       void close_video();

       void setup_encoder(enum AVCodecID, const char*);

       void close_encoder();

       void receive_frame(DataPacket* pict);

       char release_packet();

       void run();

       char fetch_packet(uint8_t** FrameBuffer, unsigned int* FrameSize);

       void set_pict(int width, int height)
       {
           PicWidth = width;
           PicHeight = height;
       }

       int pic_width() const {
           return PicWidth;
       }

       int pic_height() const {
           return PicHeight;
       }

       void set_frame_rate(double fps) {
           FrameRate = av_d2q(fps, 100);
       }

       double frame_rate(void) const {
           return av_q2d(FrameRate);
       }

       void set_bit_rate(uint64_t br) {
           BitRate = br;
       }

       void set_encoder_param(const char* name, const char* val) {
           if (name == NULL || name[0] == 0 || val == NULL || val[0] == NULL) {
               printf("setting encoder parameter failed: name or val is empty!");
           }
           av_dict_set(&EncodingOpt, name, val, 0);
       }

       uint64_t H265_bit_rate() const {
           return BitRate;
       }

       bool ready() const {
           return (State == 1);
       }

       void send_packet(AVPacket*);

       static const uint8_t* find_next_nal(const uint8_t* pbgn, const uint8_t* pend);

       const uint8_t* PPS_NAL(void) {
           if (!PPSNAL.empty())
               return PPSNAL.data_ptr();
           return NULL;
       }

       int PPS_NAL_size(void) {
           return PPSNAL.size();
       }

       const uint8_t* SPS_NAL(void) {
           if (!SPSNAL.empty())
               return SPSNAL.data_ptr();
           return NULL;
       }

       int SPS_NAL_size(void) {
           return SPSNAL.size();
       }

       const uint8_t* VPS_NAL(void) {
           if (!VPSNAL.empty())
               return VPSNAL.data_ptr();
           return NULL;
       }

       int VPS_NAL_size(void) {
           return VPSNAL.size();
       }

   private:
       DataPacket                      Pic;
       CRITICAL_SECTION                PicMutex;
       int                             PicState;

       DataPacket                      OutQueue[512];
       PacketCircularQueue<512>         OutQuePos;
       CRITICAL_SECTION                OutQueLock;

       DataPacket                      OutPack;
       AVDictionary*                   EncodingOpt;
       AVCodecContext*                 EncCtx;
       AVCodec*                        Encoder;
       AVFrame*                        SrcFrame;

       DataPacket                      SPSNAL;
       DataPacket                      PPSNAL;
       DataPacket                      VPSNAL;

       std::function<void()>           OnFrameFunc;

       AVRational                      FrameRate;
       std::clock_t                    FrameIntvl;
       uint64_t                        BitRate;
       uint64_t                        FrameInCnt;
       uint64_t                        PackOutCnt;
       int	                           PicWidth;
       int	                           PicHeight;

       char                            t_FileName[32];
       char                            State;
   };

