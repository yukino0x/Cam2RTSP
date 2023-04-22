#include "H265LiveVideoServerMediaSubssion.hh"

H265LiveVideoServerMediaSubssion* H265LiveVideoServerMediaSubssion::createNew(UsageEnvironment& env, StreamReplicator* replicator)
{
    return new H265LiveVideoServerMediaSubssion(env, replicator);
}

FramedSource* H265LiveVideoServerMediaSubssion::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
{
    estBitrate = BitRate;
    FramedSource* source = m_replicator->createStreamReplica();
    return H265VideoStreamDiscreteFramer::createNew(envir(), source);
}

RTPSink* H265LiveVideoServerMediaSubssion::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic, FramedSource* inputSource)
{
    return H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic, SPSNAL, SPSNALSize, PPSNAL, PPSNALSize, VPSNAL, VPSNALSize);
}