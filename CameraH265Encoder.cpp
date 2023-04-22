#include "CameraH265Encoder.h"
#include "ReadParams.h"
#include "libavutil/base64.h"
#include <ctime>

extern int THREAD_STATE;
extern CRITICAL_SECTION   THREAD_STATE_MUTEX;

CameraH265Encoder::CameraH265Encoder() :
	EncCtx(nullptr), SrcFrame(nullptr), EncodingOpt(nullptr),
	PackOutCnt(0), OnFrameFunc(nullptr), FrameInCnt(0)
{
	strncpy(t_FileName, "video_output_temp", 31);
	t_FileName[31] = 0;

	InitializeCriticalSectionAndSpinCount(&OutQueLock, 100);

	State = 0;
}

void CameraH265Encoder::finalise() {
	close_encoder();

	DeleteCriticalSection(&OutQueLock);

	DeleteCriticalSection(&PicMutex);
}

void CameraH265Encoder::receive_frame(DataPacket* pict)
{
	EnterCriticalSection(&PicMutex);
	Pic.swap(*pict); // swap datapacket
	PicState = 1;
	LeaveCriticalSection(&PicMutex);
	fflush(stderr);
}


void CameraH265Encoder::setup_encoder(enum AVCodecID codec_id, const char* enc_name)
{
    State = 0;

    AVDictionary*       opt = nullptr;
    AVDictionaryEntry*  dict_entry = nullptr;
    char                str_tmp[128] = { 0 };
    int ret;

    Encoder = avcodec_find_encoder_by_name(enc_name);
    if (Encoder == nullptr) {
        printf("cannot find encoder '%s', use 'libx264'", enc_name);

        Encoder = avcodec_find_encoder_by_name("libx264");
        if (Encoder == nullptr) {
            printf("failed to find codec 'libx264'!\n");
            return;
        }
    }

    if (EncCtx != nullptr) {
        avcodec_free_context(&EncCtx);
        EncCtx = nullptr;
    }

    EncCtx = avcodec_alloc_context3(Encoder);
    if (EncCtx == nullptr) {
        printf("Could not allocate video codec context");
        return;
    }

    EncCtx->bit_rate = BitRate;
    EncCtx->width = PicWidth;
    EncCtx->height = PicHeight;

    EncCtx->framerate = FrameRate;
    EncCtx->time_base = av_inv_q(FrameRate);
    EncCtx->pix_fmt = AV_PIX_FMT_YUV420P;

    EncCtx->gop_size = 10;
    EncCtx->max_b_frames = 1;

    // set AV_CODEC_FLAG_GLOBAL_HEADER will disable b_repeat_header in libx264
    EncCtx->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dict_copy(&opt, EncodingOpt, 0);

    ret = avcodec_open2(EncCtx, nullptr, &opt);
    if (ret < 0) {
        printf("open codec failed!\n");
        av_dict_free(&opt);
        return;
    }
    if (opt != nullptr) {
        printf("the following options are not used:");
        dict_entry = nullptr;
        while (dict_entry = av_dict_get(opt, "", dict_entry, AV_DICT_IGNORE_SUFFIX)) {
            fprintf(stdout, "  '%s' = '%s'\n", dict_entry->key, dict_entry->value);
        }
        av_dict_free(&opt);
    }

    if (SrcFrame != nullptr) {
        av_frame_free(&SrcFrame);
        SrcFrame = nullptr;
    };

    SrcFrame = av_frame_alloc();
    if (SrcFrame == nullptr) {
        printf("allocating input frame failed!");
        return;
    }

    SrcFrame->width = EncCtx->width; //Note Resolution must be a multiple of 2!!
    SrcFrame->height = EncCtx->height;
    SrcFrame->format = EncCtx->pix_fmt;

    ret = av_frame_get_buffer(SrcFrame, MEM_ALIGN);

    if (ret < 0) {
        printf("allocating frame data buffer failed!");
        return;
    }

    // set frame writable
    av_frame_make_writable(SrcFrame);

    // data[0]:                Y component data plane of AVFrame, which contains brightness information.
    // linesize[0]:            the number of bytes in one line of Y plane.
    // iy * linesize[0] + ix:  The address of the pixel (iy, ix) in the one-dimensional data[0] array.
    /*
    for (int iy = 0; iy < SrcFrame->height; iy++) {
        for (int ix = 0; ix < SrcFrame->width; ix++) {
            SrcFrame->data[0][iy * SrcFrame->linesize[0] + ix] = ix + iy + 3;
        }
    }

    // Cb and Cr 
    for (int iy = 0; iy < SrcFrame->height / 2; iy++) {
        for (int ix = 0; ix < SrcFrame->width; ix += 2) {
            SrcFrame->data[1][iy * SrcFrame->linesize[1] + ix] = 128 + iy;
            SrcFrame->data[1][iy * SrcFrame->linesize[1] + ix + 1] = 64 + ix;
        }
    }*/
    
    FrameInCnt = 0;
    // send dummy data to encoder
    AVPacket* pkt = av_packet_alloc();
    if (pkt == nullptr) {
        printf("allocating packet (av_packet_alloc()) failed!");
        return;
    }
    // AVRational 
    // if buffer is empty, send frame
    while (PPSNAL.empty() || SPSNAL.empty() || VPSNAL.empty()) {
        //iprintf("send a packet to encoder!");
        SrcFrame->pts = FrameInCnt;
        ret = avcodec_send_frame(EncCtx, SrcFrame);

        if (ret != 0 && ret != AVERROR(EAGAIN)) {
            printf("sending frame for encoding error!");
            return;
        }

        ++FrameInCnt;
        ret = 0;
        while (ret >= 0) {
            ret = avcodec_receive_packet(EncCtx, pkt);
            if (ret != 0) {
                if (ret != AVERROR(EAGAIN)) {
                    printf("receiving encoded packet failed!");
                }
                break;
            }
            if (pkt->size < 4) continue;
            const uint8_t* pkt_bgn = pkt->data;
            const uint8_t* pkt_end = pkt_bgn + pkt->size;
            const uint8_t* pbgn = pkt_bgn;
            const uint8_t* pend = pkt_end;
         
            while (pbgn < pkt_end) {
                pbgn = find_next_nal(pbgn, pkt_end);
                // find no NAL
                if (pbgn == pkt_end) {
                    continue;
                }
                else
                {
                    while (*pbgn == 0) ++pbgn; //skip all 0x00
                    ++pbgn; //skip 0x01
                }
                // find the end of NAL
                pend = find_next_nal(pbgn, pkt_end);
                int type = ((*pbgn) & 0x7E) >> 1;
                // cout << type << endl;
                
                if (type == 32) {     //VPS NAL
                    VPSNAL.copy(pbgn, static_cast<int>(pend - pbgn));
                }

                if (type == 33) {    //SPS NAL 
                    SPSNAL.copy(pbgn, static_cast<int>(pend - pbgn));
                }

                if (type == 34) {    //PPS NAL
                    PPSNAL.copy(pbgn, static_cast<int>(pend - pbgn));
                }
                pbgn = pend;
            }
            send_packet(pkt);
        }
    }

    FrameIntvl = (std::clock_t)(CLOCKS_PER_SEC / av_q2d(FrameRate));

    PackOutCnt = 0;

    PicState = 0;

    DeleteCriticalSection(&PicMutex);
    InitializeCriticalSectionAndSpinCount(&PicMutex, 1000);
    State = 1;
}

void CameraH265Encoder::run() 
{
    int ret;
    clock_t clk_pre = std::clock() - FrameIntvl; // stores the system time of the previous frame.
    clock_t clk_cur;
    unsigned int frame_cnt = 0;

    // CLOCKS_PER_SEC: the number of clock cycles the CPU runs in one second

    clock_t intvl = CLOCKS_PER_SEC / 1000; // clock cycles per msec

    double clk2msec = 1000 / (double)CLOCKS_PER_SEC; // Convert CPU execution time from the number of clock cycles to msec
    long msec;

    AVPacket* pkt = av_packet_alloc();
    if (pkt == nullptr) {
        printf("allocating packet (av_packet_alloc()) failed!");
        return;
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(AV_PIX_FMT_YUV420P);

    AVFrame* frame = av_frame_alloc();
    if (frame == nullptr) {
        printf("allocating frame (av_frame_alloc()) failed!");
        return;
    }
    frame->width = PicWidth;
    frame->height = PicHeight;
    frame->format = AV_PIX_FMT_YUV420P;
    int dataInSz = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, PicWidth,
        PicHeight, MEM_ALIGN);

    DataPacket pack_in(dataInSz);

    DataPacket pack_out;

    while (true) 
    {
        if (THREAD_STATE >0 )
        {   
            break;
        }
        if (PicState == 1) {
            if (Pic.size() != dataInSz) {
                printf("incoming data size is not right! in_data_sz = %d, frame_data_sz = %d",
                    Pic.size(), dataInSz);
            }

            // try to enter picmutex
            if (TryEnterCriticalSection(&PicMutex) == 0) {
                Sleep(1);
                if (TryEnterCriticalSection(&PicMutex) == 0) {
                    continue;
                }
            }
            pack_in.swap(Pic);
            PicState = 0;
            LeaveCriticalSection(&PicMutex);
            // leave critical section - consume!

            av_image_fill_arrays(frame->data, frame->linesize, pack_in.data_ptr(),
                (enum AVPixelFormat)frame->format, frame->width, frame->height, MEM_ALIGN);

            ++frame_cnt;

            uint8_t* src_ptr;
            uint8_t* dst_ptr;
            // y plane, copy by line
            for (int iy = 0; iy < frame->height; ++iy) {
                dst_ptr = &(SrcFrame->data[0][iy * SrcFrame->linesize[0]]);
                src_ptr = &(frame->data[0][iy * frame->linesize[0]]);
                memcpy(dst_ptr, src_ptr, frame->linesize[0]);
            }

            // u plane, copy by line
            for (int iy = 0; iy < frame->height / 2; ++iy) {
                dst_ptr = &(SrcFrame->data[1][iy * SrcFrame->linesize[1]]);
                src_ptr = &(frame->data[1][iy * frame->linesize[1]]);
                memcpy(dst_ptr, src_ptr, frame->linesize[1]);
            }

            // v plane, copy by line
            for (int iy = 0; iy < frame->height / 2; ++iy) {
                dst_ptr = &(SrcFrame->data[2][iy * SrcFrame->linesize[2]]);
                src_ptr = &(frame->data[2][iy * frame->linesize[2]]);
                memcpy(dst_ptr, src_ptr, frame->linesize[2]);
            }
        }

        SrcFrame->pts = FrameInCnt;
        SrcFrame->display_picture_number = static_cast<int>(FrameInCnt % 0x7FFFFFFF);

        clk_cur = std::clock();

        if (clk_cur - clk_pre + intvl < FrameIntvl) {
            msec = static_cast<long>(clk2msec * (clk_cur - clk_pre + intvl));
            if (msec > 0 && msec < FrameIntvl) {
                // printf("sleep for %d msec. ", msec);
                Sleep(msec);
            }
        }
        clk_pre = clk_cur;

        ret = avcodec_send_frame(EncCtx, SrcFrame);

        if (ret != 0 && ret != AVERROR(EAGAIN)) {
            printf("sending frame for encoding error!");
            return;
        }
        //iprintf("send a packet to encoder!");

        ++FrameInCnt;

        ret = 0;
        while (ret >= 0) 
        {
            ret = avcodec_receive_packet(EncCtx, pkt);
            if (ret != 0) {
                if (ret != AVERROR(EAGAIN)) {
                    printf("receiving encoded packet failed!");
                }
                break;
            }
            send_packet(pkt);
        }
    }
    av_frame_free(&frame);
    av_packet_free(&pkt);

}


void CameraH265Encoder::send_packet(AVPacket* avpkt)
{
    static DataPacket data_out;

    /* remove nal_unit_header */
    uint8_t* ptr = avpkt->data;
    int pkt_len = avpkt->size;
    if (ptr[0] == 0x00 && ptr[1] == 0x00) {
        // 00 00 01
        if (ptr[2] == 0x01) {
            ptr += 3;
            pkt_len -= 3;
        }
        // 00 00 00 01
        else if ((ptr[2] == 0x00) && (ptr[3] == 0x01)) {
            ptr += 4;
            pkt_len -= 4;
        }
    }

    if (pkt_len <= 0)
        return;

    data_out.copy(ptr, pkt_len); //copy dataptr and len to data_out

    EnterCriticalSection(&OutQueLock);
    if (OutQuePos.full()) {
        OutQuePos.pop_front();
    }

    OutQueue[OutQuePos.push_back()].swap(data_out); //insert data_out to queue
    LeaveCriticalSection(&OutQueLock);
    // callback function
    if (OnFrameFunc != nullptr) {
        OnFrameFunc();
    }
}

void CameraH265Encoder::close_encoder()
{

    if (EncodingOpt) {
        av_dict_free(&EncodingOpt);
    }

    if (EncCtx != nullptr) {
        avcodec_free_context(&EncCtx);
        EncCtx = nullptr;
    }

    if (SrcFrame != nullptr) {
        av_frame_free(&SrcFrame);
        SrcFrame = nullptr;
    }
}

void CameraH265Encoder::close_video()
{
    close_encoder();
}


char CameraH265Encoder::fetch_packet(uint8_t** FrameBuffer, unsigned int* FrameSize)
{
    if (!OutQuePos.empty())
    {
        EnterCriticalSection(&OutQueLock);
        size_t pos = OutQuePos.front();
        OutPack.swap(OutQueue[pos]);
        OutQuePos.pop_front();
        LeaveCriticalSection(&OutQueLock);

        OutPack.set_ref_id(1);
        ++PackOutCnt;
        *FrameBuffer = OutPack.data_ptr();
        *FrameSize = OutPack.size();
        //iprintf("a frame is delivered to RTSP server! sz = %d", *FrameSize);
        return 1;
    }
    else {
        *FrameBuffer = nullptr;
        *FrameSize = 0;
        return 0;
    }
}

char CameraH265Encoder::release_packet()
{
    OutPack.set_ref_id(0);
    return 1;
}

const uint8_t* CameraH265Encoder::find_next_nal(const uint8_t* start, const uint8_t* end)
{
    const uint8_t* p = start;

    /* Simply lookup "0x 00 00 01" pattern */
    // ensure there are 3 bytes to check in nal
    while (p <= end - 3) {
        if (p[0] == 0x00) {             // first byte
            if (p[1] == 0x00) {         // second byte
                if ((p[2] == 0x01) || (p[2] == 0x00 && p[3] == 0x01)) { // 00 00 01 or 00 00 00 01
                    return p;
                }
                else {
                    // skip 3 bytes
                    p += 3;
                }
            }
            else {
                p += 2;
            }
        }
        else {
            ++p;
        }
    }
    return end;
}
