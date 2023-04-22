#include "BasicUsageEnvironment.hh"
#include "UsageEnvironment.hh"
#include "CameraCapture.h"
#include "ReadParams.h"
#include "DataPacket.h"
#include "CameraH265Encoder.h"
#include "H265RtspServer.h"

int                THREAD_STATE = 0;
CRITICAL_SECTION   THREAD_STATE_MUTEX;

CameraH265Encoder* H265Encoder = nullptr;
H265RtspServer*    RTSPServer = nullptr;
CameraCapture*	   CamCap =  nullptr ;

int                UDPPort;
int                HTTPTunnelPort;


DWORD WINAPI runRTSPServer(LPVOID RTSPServer)
{
	((H265RtspServer*)RTSPServer)->run();
	EnterCriticalSection(&THREAD_STATE_MUTEX);
	THREAD_STATE = 1; //indicate a thread is terminated
	LeaveCriticalSection(&THREAD_STATE_MUTEX);
	printf("RTSP server thread finished!");
	ExitThread(NULL);
}

DWORD WINAPI runCapture(LPVOID cam)
{
	((CameraCapture*)cam)->capture();
	EnterCriticalSection(&THREAD_STATE_MUTEX);
	THREAD_STATE = 1; //indicate a thread is terminated
	LeaveCriticalSection(&THREAD_STATE_MUTEX);
	printf("campturing thread finished!");
	ExitThread(NULL);
}

// call back function
void CamOnFrame(DataPacket* buff)
{
	H265Encoder->receive_frame(buff);
}
 
int main()
{

	avdevice_register_all();
	av_log_set_level(AV_LOG_ERROR);

	// reading params from txt file
	ReadParams read_param("config.txt");
	CameraParams cam_param;
	cam_param.setParms(read_param);

	InitializeCriticalSectionAndSpinCount(&THREAD_STATE_MUTEX, 100);

	HANDLE cam_thread_id = nullptr ;
	HANDLE rtsp_thread_id = nullptr;

	H265Encoder = new CameraH265Encoder();
	if (H265Encoder == nullptr) {
		printf("allocating h265 encoder failed!");
		goto FINISHED;
	}
	
	H265Encoder->set_pict(cam_param.get_width(), cam_param.get_height());
	H265Encoder->set_bit_rate(cam_param.get_bitrate());
	H265Encoder->set_frame_rate(cam_param.get_bitrate());
	H265Encoder->setup_encoder(AV_CODEC_ID_H265, cam_param.get_encoder().c_str());

	if (!H265Encoder->ready()) {
		printf("h265 encoder is not ready!");
		goto FINISHED;
	}

	cam_thread_id = nullptr;
	CamCap = new CameraCapture(&cam_param);
	if (!CamCap) {
		printf("failed to create a object for %s", cam_param.get_name().c_str());
		goto FINISHED;
	}

	CamCap->set_callback_func(CamOnFrame);
	CamCap->initialize();
	if (!CamCap->ready()) {
		printf("initialising camera '%s' failed!", cam_param.get_name().c_str());
		delete CamCap;
		CamCap = nullptr;
		goto FINISHED;
	}

	RTSPServer = new H265RtspServer(H265Encoder, 8554, 8080);

	if (CamCap) {
		cam_thread_id = CreateThread(nullptr,    // default security attributes
			0,                                    // default stack size
			(LPTHREAD_START_ROUTINE)runCapture,   //the routine
			CamCap,								//function argument
			0,                                    //default creation flags
			nullptr);

		if (cam_thread_id == nullptr) {
			//exception
			printf("creating thread for capturing '%s' failed!", cam_param.get_name().c_str());
			goto FINISHED;
		}
	}
	else {
		cam_thread_id = nullptr;
	}

	rtsp_thread_id = CreateThread(nullptr, // default security attributes
		0,                               // default stack size
		(LPTHREAD_START_ROUTINE)runRTSPServer, //the routine
		RTSPServer,                      //function argument
		0,                               //default creation flags
		nullptr);


	if (rtsp_thread_id == nullptr) {
		//exception
		printf("creating thread for RTSP server failed!");
		goto FINISHED;
	}
	// Play Media Here
	H265Encoder->run();

	EnterCriticalSection(&THREAD_STATE_MUTEX);
	THREAD_STATE = 1; //indicate a thread is terminated
	LeaveCriticalSection(&THREAD_STATE_MUTEX);
	printf("encoding thread finished!");


FINISHED:
	if (RTSPServer != nullptr) {
		RTSPServer->signal_exit();
	}

	if (H265Encoder != nullptr) {
		delete H265Encoder;
	}

	Sleep(1000);  //wait for the thread to exit;

	DWORD exit_code;
	if (RTSPServer != nullptr) {
		if (rtsp_thread_id != nullptr && GetExitCodeThread(rtsp_thread_id, &exit_code) && exit_code == STILL_ACTIVE) {
			printf("RTSP server thread still running, kill it!");
			TerminateThread(rtsp_thread_id, -1);
		}
		delete RTSPServer;
	}

	if (CamCap != nullptr) {
		if (cam_thread_id != nullptr && GetExitCodeThread(cam_thread_id, &exit_code) && exit_code == STILL_ACTIVE) {
			printf("thread for camera '%s' still running, kill it!", cam_param.get_name().c_str());
			TerminateThread(cam_thread_id, -1);
		}
		delete CamCap;
	}
	
	DeleteCriticalSection(&THREAD_STATE_MUTEX);
}