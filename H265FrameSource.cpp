#include "H265FrameSource.h"
H265FrameSource* H265FrameSource::createNew(UsageEnvironment& env, CameraH265Encoder* encoder)
{
    return new H265FrameSource(env, encoder);
}

H265FrameSource::H265FrameSource(UsageEnvironment& env, CameraH265Encoder* encoder) :
    FramedSource(env), Encoder(encoder)
{
    EventTriggerId = envir().taskScheduler().createEventTrigger(H265FrameSource::deliverFrameStub);
    std::function<void()> callbackfunc = std::bind(&H265FrameSource::onFrame, this);
    Encoder->set_callback_func(callbackfunc);
}


void H265FrameSource::doStopGettingFrames()
{
    FramedSource::doStopGettingFrames();
}

void H265FrameSource::onFrame()
{
    envir().taskScheduler().triggerEvent(EventTriggerId, this);
}

void H265FrameSource::doGetNextFrame()
{
    deliverFrame();
}

void H265FrameSource::deliverFrame()
{
    if (!isCurrentlyAwaitingData()) return; // we're not ready for the buff yet

    static uint8_t* newFrameDataStart;
    static unsigned newFrameSize = 0;

    /* get the buff frame from the Encoding thread.. */
    if (Encoder->fetch_packet(&newFrameDataStart, &newFrameSize)) {
        if (newFrameDataStart != nullptr) {
            /* This should never happen, but check anyway.. */
            if (newFrameSize > fMaxSize) {
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = newFrameSize - fMaxSize;
            }
            else {
                fFrameSize = newFrameSize;
            }

            gettimeofday(&fPresentationTime, nullptr);
            memcpy(fTo, newFrameDataStart, fFrameSize);

            //delete newFrameDataStart;
            //newFrameSize = 0;

            Encoder->release_packet();
        }
        else {
            fFrameSize = 0;
            fTo = nullptr;
            handleClosure(this);
        }
    }
    else {
        fFrameSize = 0;
    }

    if (fFrameSize > 0)
        FramedSource::afterGetting(this);

}