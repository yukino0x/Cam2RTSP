#pragma once
#include "ReadParams.h"
#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "liveMedia.hh"
#include "H265LiveVideoServerMediaSubssion.hh"
#include "H265FrameSource.h"
#include "CameraH265Encoder.h"

class H265RtspServer
{
public:

   H265RtspServer(CameraH265Encoder * enc, int port, int httpPort);
   ~H265RtspServer();

   void run();

   void signal_exit() {
      Quit = 1;
   }

   void set_bit_rate(uint64_t br) {
      if (br < 102400) {
         BitRate = 100;
      }
      else {
         BitRate = static_cast<unsigned int>(br / 1024); //in kbs
      }
   }

private:
   int                 PortNum;
   int                 HttpTunnelingPort;
   unsigned int        BitRate;
   CameraH265Encoder*  H265Encoder;
   char                Quit;
};