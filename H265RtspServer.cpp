
#include "H265RtspServer.h"

extern int THREAD_STATE;
extern CRITICAL_SECTION   THREAD_STATE_MUTEX;

H265RtspServer::H265RtspServer(CameraH265Encoder* enc, int port, int httpPort)
    : H265Encoder(enc), PortNum(port), HttpTunnelingPort(httpPort), Quit(0), BitRate(5120) //in kbs
{
    Quit = 0;
};

H265RtspServer::~H265RtspServer()
{
};

void H265RtspServer::run()
{
    TaskScheduler* scheduler;
    UsageEnvironment* env;
    char stream_name[] = "mystream";

    scheduler = BasicTaskScheduler::createNew();
    env = BasicUsageEnvironment::createNew(*scheduler);

    UserAuthenticationDatabase* authDB = nullptr;

    // if (m_Enable_Pass){
    // 	authDB = new UserAuthenticationDatabase;
    // 	authDB->addUserRecord(UserN, PassW);
    // }

    OutPacketBuffer::increaseMaxSizeTo(52428800);  // 50M
    RTSPServer* rtspServer = RTSPServer::createNew(*env, PortNum, authDB);

    if (rtspServer == nullptr)
    {
        *env << "LIVE555: Failed to create RTSP server: %s\n", env->getResultMsg();
    }
    else {
        if (HttpTunnelingPort)
        {
            rtspServer->setUpTunnelingOverHTTP(HttpTunnelingPort);
        }

        char const* descriptionString = "Combined Multiple Cameras Streaming Session";

        H265FrameSource* source = H265FrameSource::createNew(*env, H265Encoder);
        StreamReplicator* inputDevice = StreamReplicator::createNew(*env, source, false);

        ServerMediaSession* sms = ServerMediaSession::createNew(*env, stream_name, stream_name, descriptionString);
        H265LiveVideoServerMediaSubssion* sub = H265LiveVideoServerMediaSubssion::createNew(*env, inputDevice);

        sub->set_PPS_NAL(H265Encoder->PPS_NAL(), H265Encoder->PPS_NAL_size());
        sub->set_SPS_NAL(H265Encoder->SPS_NAL(), H265Encoder->SPS_NAL_size());
        sub->set_VPS_NAL(H265Encoder->VPS_NAL(), H265Encoder->VPS_NAL_size());
        if (H265Encoder->H265_bit_rate() > 102400) {
            sub->set_bit_rate(H265Encoder->H265_bit_rate());
        }
        else {
            sub->set_bit_rate(static_cast<uint64_t>(H265Encoder->pic_width() * H265Encoder->pic_height() * H265Encoder->frame_rate() / 20));
        }

        sms->addSubsession(sub);
        rtspServer->addServerMediaSession(sms);

        char* url = rtspServer->rtspURL(sms);
        *env << "Play this stream using the URL \"" << url << "\"\n";
        delete[] url;

        //signal(SIGNIT,sighandler);
        env->taskScheduler().doEventLoop(&Quit); // does not return

        Medium::close(rtspServer);
        Medium::close(inputDevice);
    }

    env->reclaim();
    delete scheduler;
}