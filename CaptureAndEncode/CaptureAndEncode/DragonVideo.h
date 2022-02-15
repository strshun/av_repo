#ifndef _DRAGONVIDEO_112B724A_8779_46D0_9501_552026B5E82A_H_
#define _DRAGONVIDEO_112B724A_8779_46D0_9501_552026B5E82A_H_


#ifdef __cplusplus
extern "C" {
#endif

#ifndef DL_EXPORT
#define DL_EXPORT _declspec(dllexport) 
#endif

//���������ʶ
#define __INOUT__ 

	//@desc ����
	typedef void(*PFNONVIDEOERROR)(int errCode, const char* errMsg, void* userData);
	//@desc д��־
	//@param file �����ļ�
	//@param line ������
	//@param logLevel 0-��ͨ 1-���� 2-����
	//@param userData �û�����
	//@parame fmt��־����
	typedef void(*PFNONVIDEOLOG)(const char* file, const char* line, int logLevel, void* userData, const char* fmt, ...);

	typedef struct tagVIDEOCALLBACKS_ {
		PFNONVIDEOERROR OnError;
		PFNONVIDEOLOG OnLog;
	}VIDEOCALLBACKS, *PVIDEOCALLBACKS;

	//@desc ע����Ƶ�ص�
	typedef void(*PFNREGISTERVIDEOCALLBACK)(PVIDEOCALLBACKS callbacks, void* userData);
	typedef struct tagVIDEOENCPARAMS_ {
		int encodeWidth;		//�����
		int encodeHeight;		//�����
		int videoForamt;		//�����ʽ 1��ʾvp8,2��ʾh264,3��ʾh265
		int bitrate;			//������
		int bitateMode;			//�����ʿ���ģʽ 1-�̶� 2-��̬
		int maxFps;				//���֡��		
		int fpsMode;			//֡�ʿ���ģʽ	1-�̶�֡ 2-��̬֡
		int clientType;			//�ͻ�������		1-��׿
		int outputDelay;		//����������ӳ�
#ifdef DRAGON_MULTIPLE_ENCODE
		int clientId;           //�ͻ���id
#endif
	}VIDEOENCPARAMS, *PVIDEOENCPARAMS;
	//@desc ������Ƶ���������
	//@param params ��Ƶ����������������
	typedef int(*PFNCREATEVIDEOPROCESSOR)(__INOUT__ PVIDEOENCPARAMS params);
	//@desc ������Ƶ���������
	//@param params ��Ƶ����������������
	typedef int(*PFNRESETVIDEOPROCESSOR)(__INOUT__ PVIDEOENCPARAMS params);
	//@desc ������Ƶ
	typedef void(*PFNDESTROYVIDEOPROCESSOR)();


	typedef struct tagDXGIFRAME_ {
		int frameType;
		unsigned char* frameBuffer;
		unsigned char* sendBuffer;
		int frameSize;
		int frameWidth;
		int frameHeight;

		unsigned long capStartTime;	//��ʼ�����ʱ��
		unsigned long capEndTime;	//��������ʼ�����ʱ��
		unsigned long encEndTime;	//���������ʱ��
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
