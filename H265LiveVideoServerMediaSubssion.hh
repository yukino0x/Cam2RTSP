
#ifndef _H265_LIVE_VIDEO_SERVER_MEDIA_SUBSESSION_HH
#define _H265_LIVE_VIDEO_SERVER_MEDIA_SUBSESSION_HH

#include "ReadParams.h"
#include "OnDemandServerMediaSubsession.hh"
#include "StreamReplicator.hh"
#include "H265VideoRTPSink.hh"
#include "H265VideoStreamFramer.hh"
#include "H265VideoStreamDiscreteFramer.hh"
#include "UsageEnvironment.hh"
#include "Groupsock.hh"
class H265LiveVideoServerMediaSubssion : public OnDemandServerMediaSubsession {

public:
    static H265LiveVideoServerMediaSubssion* createNew(UsageEnvironment& env, StreamReplicator* replicator);

    void set_PPS_NAL(const uint8_t* ptr, int size) {
        PPSNAL = ptr;
        PPSNALSize = size;
    }

    void set_SPS_NAL(const uint8_t* ptr, int size) {
        SPSNAL = ptr;
        SPSNALSize = size;
    }

    void set_VPS_NAL(const uint8_t* ptr, int size)
    {
        VPSNAL = ptr;
        VPSNALSize = size;
    }
    void set_bit_rate(uint64_t br) {
        if (br > 102400) {
            BitRate = static_cast<unsigned long>(br / 1024);
        }
        else {
            printf("estimated bit rate %lld too small, set to 100K!", br);
            BitRate = 100;
        }
    }


protected:
    // reuseFirstSource: True
    H265LiveVideoServerMediaSubssion(UsageEnvironment& env, StreamReplicator* replicator)
        : OnDemandServerMediaSubsession(env, True), m_replicator(replicator), BitRate(1024), PPSNAL(nullptr), PPSNALSize(0),
        SPSNAL(nullptr), SPSNALSize(0), VPSNAL(nullptr), VPSNALSize(0)
    {
    };

    FramedSource* createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate) override;
    RTPSink* createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource) override;

    StreamReplicator*   m_replicator;
    unsigned long       BitRate;
    const uint8_t*      PPSNAL;
    int                 PPSNALSize;
    const uint8_t*      SPSNAL;
    int                 SPSNALSize;
    const uint8_t*      VPSNAL;
    int                 VPSNALSize;
};


#endif