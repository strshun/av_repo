#ifndef _DRAGONVIDEO_112B724A_8779_46D0_9501_552026B5E82A_H_
#define _DRAGONVIDEO_112B724A_8779_46D0_9501_552026B5E82A_H_


#ifdef __cplusplus
extern "C" {
#endif

#ifndef DL_EXPORT
#define DL_EXPORT _declspec(dllexport) 
#endif

//输入输出标识
#define __INOUT__ 

	//@desc 错误
	typedef void(*PFNONVIDEOERROR)(int errCode, const char* errMsg, void* userData);
	//@desc 写日志
	//@param file 所在文件
	//@param line 所在行
	//@param logLevel 0-普通 1-警告 2-错误
	//@param userData 用户数据
	//@parame fmt日志内容
	typedef void(*PFNONVIDEOLOG)(const char* file, const char* line, int logLevel, void* userData, const char* fmt, ...);

	typedef struct tagVIDEOCALLBACKS_ {
		PFNONVIDEOERROR OnError;
		PFNONVIDEOLOG OnLog;
	}VIDEOCALLBACKS, *PVIDEOCALLBACKS;

	//@desc 注册视频回调
	typedef void(*PFNREGISTERVIDEOCALLBACK)(PVIDEOCALLBACKS callbacks, void* userData);
	typedef struct tagVIDEOENCPARAMS_ {
		int encodeWidth;		//编码宽
		int encodeHeight;		//编码高
		int videoForamt;		//编码格式 1表示vp8,2表示h264,3表示h265
		int bitrate;			//比特率
		int bitateMode;			//比特率控制模式 1-固定 2-动态
		int maxFps;				//最大帧率		
		int fpsMode;			//帧率控制模式	1-固定帧 2-动态帧
		int clientType;			//客户端类型		1-安卓
		int outputDelay;		//编码器输出延迟
#ifdef DRAGON_MULTIPLE_ENCODE
		int clientId;           //客户端id
#endif
	}VIDEOENCPARAMS, *PVIDEOENCPARAMS;
	//@desc 创建视频捕获编码器
	//@param params 视频处理参数，输入输出
	typedef int(*PFNCREATEVIDEOPROCESSOR)(__INOUT__ PVIDEOENCPARAMS params);
	//@desc 重置视频捕获编码器
	//@param params 视频处理参数，输入输出
	typedef int(*PFNRESETVIDEOPROCESSOR)(__INOUT__ PVIDEOENCPARAMS params);
	//@desc 销毁视频
	typedef void(*PFNDESTROYVIDEOPROCESSOR)();


	typedef struct tagDXGIFRAME_ {
		int frameType;
		unsigned char* frameBuffer;
		unsigned char* sendBuffer;
		int frameSize;
		int frameWidth;
		int frameHeight;

		unsigned long capStartTime;	//开始捕获的时间
		unsigned long capEndTime;	//结束捕获开始编码的时间
		unsigned long encEndTime;	//结束编码的时间
	}DXGIFRAME, *PDXGIFRAME;


#ifdef DRAGON_MULTIPLE_ENCODE
	typedef int(*PFNACQUIREFRAMEMulEncode)(PDXGIFRAME frame, bool withCursor, int* cursorType, const int clientId);
#else
	typedef int(*PFNACQUIREFRAME)(PDXGIFRAME frame, bool withCursor, int* cursorType);
#endif

#ifdef DRAGON_MULTIPLE_ENCODE
	typedef void(*PFNCLOSEVIDEOENCODE)(const int clientId);
#endif

	typedef struct tagDRAGONVIDEOAPIS_ {
		int version;
		PFNREGISTERVIDEOCALLBACK RegisterCallbacks;
		PFNCREATEVIDEOPROCESSOR CreateVideoProcessor;
		PFNRESETVIDEOPROCESSOR ResetVideoProcessor;
		PFNDESTROYVIDEOPROCESSOR DestroyVideoProcessor;
#ifdef DRAGON_MULTIPLE_ENCODE
		PFNACQUIREFRAMEMulEncode AcquireFrameMulEncode;
#else
		PFNACQUIREFRAME AcquireFrame;
#endif

#ifdef DRAGON_MULTIPLE_ENCODE
		PFNCLOSEVIDEOENCODE CloseVideoEncode;
#endif
	}DRAGONVIDEOAPIS, *PDRAGONVIDEOAPIS;


	DL_EXPORT void* InitDragonVideo(PDRAGONVIDEOAPIS apis);

	DL_EXPORT void UninitDragonVideo(void* instance);

#ifdef __cplusplus
}
#endif



#endif
