#pragma once
#ifndef _H265FRAMESOURCE_H_
#define _H265FRAMESOURCE_H_

#include "ReadParams.h"
#include "FramedSource.hh"
#include "UsageEnvironment.hh"
#include "Groupsock.hh"
#include "GroupsockHelper.hh"
#include "CameraH265Encoder.h"

class H265FrameSource : public FramedSource {
public:

    static H265FrameSource* createNew(UsageEnvironment&, CameraH265Encoder*);
    H265FrameSource(UsageEnvironment& env, CameraH265Encoder*);
    ~H265FrameSource() = default;

private:
    // call the specified object's deliverFrame() method via clientData.
    static void deliverFrameStub(void* clientData) {
        ((H265FrameSource*)clientData)->deliverFrame();
    };
    void doGetNextFrame() override;
    void deliverFrame();
    void doStopGettingFrames() override;
    void onFrame();

private:
    CameraH265Encoder*   Encoder;
    EventTriggerId       EventTriggerId;
};

#endif