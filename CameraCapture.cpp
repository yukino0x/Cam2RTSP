#include "CameraCapture.h"
#include "ReadParams.h"



extern int THREAD_STATE;
extern CRITICAL_SECTION   THREAD_STATE_MUTEX;

CameraCapture::CameraCapture(const CameraParams* params) :
	OnFrameFunc(nullptr), Input_fmt_Ctx(nullptr), CodecCtx(nullptr),
	CamInDict(nullptr), SwsCtx(nullptr), FrameOut(nullptr), Bitrate(5120000),
	FrameCnt(0), State(0), CamParams(params), DelayClock(CLOCKS_PER_SEC / 15)
{

};

int CameraCapture::initialize() {
	State = 0;

	// record return value for ffmpeg function
	int ret = 0;

	// temp string for logging, data copy... etc.
	char str_tmp[256];
	str_tmp[255] = 0;

	AVDictionary* opt = nullptr;
	AVDictionaryEntry* dict_entry = nullptr;
	AVInputFormat* in_fmt = nullptr;

	avdevice_register_all();
	// av_log_set_level(AV_LOG_ERROR);

	if (CodecCtx != nullptr) {
		avcodec_free_context(&CodecCtx);
		CodecCtx = nullptr;
	}

	if (Input_fmt_Ctx != nullptr) {
		avformat_close_input(&Input_fmt_Ctx);
		Input_fmt_Ctx = nullptr;
	}

	if (FrameOut != nullptr) {
		if (FrameOut->data[0] != nullptr) {
			av_freep(FrameOut->data[0]);
		}
		av_frame_free(&FrameOut);
		FrameOut = nullptr;
	}

	Input_fmt_Ctx = avformat_alloc_context();

	in_fmt = av_find_input_format("dshow");

	ret = snprintf(str_tmp, 255, "video_size=%dx%d;framerate=%g;rtbufsize=%llu;",
		CamParams->get_width(), CamParams->get_height(), CamParams->get_framerate(), CamParams->get_buffer());

	ret = av_dict_parse_string(&opt, str_tmp, "=", ";", 0);

	snprintf(str_tmp, 255, "video=%s", CamParams->get_name().c_str());

	if (ret = avformat_open_input(&Input_fmt_Ctx, str_tmp, in_fmt, &opt) != 0) {
		//exception
		av_dict_free(&opt);
		av_strerror(ret, str_tmp, 255);
		printf("open video input '%s' failed! err = %s", CamParams->get_name().c_str(), str_tmp);
		return -1;
	}

	av_dump_format(Input_fmt_Ctx, 0, Input_fmt_Ctx->url, 0);

	if (opt != nullptr) {
		printf("the following options are not used:");
		dict_entry = nullptr;
		while (dict_entry = av_dict_get(opt, "", dict_entry, AV_DICT_IGNORE_SUFFIX)) {
			fprintf(stdout, "  '%s' = '%s'\n", dict_entry->key, dict_entry->value);
		}
		av_dict_free(&opt);
	}

	if (avformat_find_stream_info(Input_fmt_Ctx, nullptr) < 0) {
		//exception
		printf("getting audio/video stream information from '%s' failed!", CamParams->get_name().c_str());
		return -1;
	}

	//print out the video stream information
	printf("video stream from: '%s'\n", CamParams->get_name().c_str());

	OutStreamNum = -1;
	AVStream* stream = nullptr;
	for (unsigned int i = 0; i < Input_fmt_Ctx->nb_streams; i++) {
		if (Input_fmt_Ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			OutStreamNum = i;
			stream = Input_fmt_Ctx->streams[i];
		}
	}

	if (OutStreamNum == -1 || stream == nullptr) {
		//exception
		printf("no video stream found in '%s'!\n", CamParams->get_name().c_str());
		return -1;
	}

	AVCodecParameters* codec_par = stream->codecpar;

	Bitrate = codec_par->bit_rate;

	DelayClock = (int)((double)(CLOCKS_PER_SEC) * (double)(stream->avg_frame_rate.den) / (double)(stream->avg_frame_rate.num));

	AVCodec* decoder = avcodec_find_decoder(codec_par->codec_id);
	if (decoder == nullptr) {
		printf("cannot find decoder for '%s'!", avcodec_get_name(codec_par->codec_id));
		return -1;
	}

	// cout << "decoder is: " << avcodec_get_name(codec_par->codec_id) << endl;

	CodecCtx = avcodec_alloc_context3(decoder);

	avcodec_parameters_to_context(CodecCtx, codec_par);

	// log level: Something went wrong and recovery is not possible.
	av_log_set_level(AV_LOG_FATAL);
	if (avcodec_open2(CodecCtx, decoder, nullptr) < 0) {
		printf("Cannot open video decoder for camera!");
		return -1;
	}

	// sws_getCachedContext: reinitialize or create a new SwsContext struct
	SwsCtx = sws_getCachedContext(nullptr, CodecCtx->width, CodecCtx->height, CodecCtx->pix_fmt,
			CodecCtx->width, CodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, nullptr, nullptr, nullptr);

	FrameOut = av_frame_alloc();
	if (FrameOut == nullptr) {
		printf("allocating output frame failed!");
		return -1;
	}

	FrameOut->width = CodecCtx->width;
	FrameOut->height = CodecCtx->height;
	FrameOut->format = AV_PIX_FMT_YUV420P;
	DataOutSz = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, FrameOut->width, FrameOut->height, MEM_ALIGN);  
	ret = av_frame_get_buffer(FrameOut, MEM_ALIGN);
	if (ret < 0) {
		printf("allocating frame data buffer failed!");
		return -1;
	}

	// printf("data out sz is: %d", DataOutSz); // 3110400
	DataOut.resize(DataOutSz);
	State = 1;
	return 0;

}

// start capture the frame
void CameraCapture::capture() {
	if (!State) return;

	// record return value for ffmpeg function
	int ret;

	int packet_cnt = 0;

	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();

	while (true) {
		if (THREAD_STATE > 0) break; // if the thread is using, break

		ret = av_read_frame(Input_fmt_Ctx, packet);
		if (ret < 0) {
			break;
		}
		// cout << "pkt dts: " << packet->dts << " ----- pkt dts: " << packet->pts << endl;

		++packet_cnt;

		if (packet->buf == nullptr || packet->stream_index != OutStreamNum)
			continue;

		ret = avcodec_send_packet(CodecCtx, packet);
		if (ret != 0 && ret == AVERROR(EAGAIN)) {
			printf("sending a packet for decoding failed!");
			return;
		}

		do {
			ret = avcodec_receive_frame(CodecCtx, frame);

			if (ret == AVERROR(EAGAIN)) {
				break;
			}
			else if (ret != 0) {
				printf("reading a frame from decoder failed!");
				return;
			}

			ret = sws_scale(SwsCtx, frame->data, frame->linesize, 0, frame->height,
				FrameOut->data, FrameOut->linesize);
			if (ret <= 0) {
				printf("scaling picture failed!");
				return;
			}

			DataOut.resize(DataOutSz);

			av_image_copy_to_buffer(DataOut.data_ptr(), DataOut.size(), FrameOut->data,
				FrameOut->linesize, (enum AVPixelFormat)FrameOut->format,
				FrameOut->width, FrameOut->height, MEM_ALIGN);

			//DataOut.copy(FrameOut->data[0], DataOutSz);
			++FrameCnt;
			if (OnFrameFunc != nullptr) {
				OnFrameFunc(&DataOut);
			}

			//iprintf("send a frame to encoder, size = %d.", frame->pkt_size);
		} while (ret >= 0);

		Sleep(1);

		av_packet_unref(packet);
		av_frame_unref(frame);
	}

	av_frame_free(&frame);
	av_packet_free(&packet);
}


