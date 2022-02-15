#include "DragonVideo.h"
#include "VideoCapture.h"

static int m_currentVersion;
std::unique_ptr<Dragon::VideoCapture> m_videoCapture;
static PVIDEOCALLBACKS m_videoCallbacks;
static void * m_userData;

static int CreateVideoProcessor(PVIDEOENCPARAMS params)
{
	Dragon::VideoCapture::VideoConfig config;
	config.bitateMode = params->bitateMode;
	config.bitrate = params->bitrate;
	config.encodeHeight = params->encodeHeight;
	config.encodeWidth = params->encodeWidth;
	config.fpsMode = params->fpsMode;
	config.videoForamt = params->videoForamt;
	config.maxFps = params->maxFps;
	config.clientType = params->clientType;
	config.outputDelay = params->outputDelay;
	m_videoCapture = Dragon::VideoCapture::Create(config);
	if (!m_videoCapture.get())
	{
		return -1;
	}
	return 0;
}

static int ResetVideoProcessor(PVIDEOENCPARAMS params)
{
	Dragon::VideoCapture::VideoConfig config;
	config.bitateMode = params->bitateMode;
	config.bitrate = params->bitrate;
	config.encodeHeight = params->encodeHeight;
	config.encodeWidth = params->encodeWidth;
	config.fpsMode = params->fpsMode;
	config.videoForamt = params->videoForamt;
	config.maxFps = params->maxFps;
	config.clientType = params->clientType;
	config.outputDelay = params->outputDelay;
	bool err = m_videoCapture->ReconfigVideoCapture(config);
	if (!err)
	{
		return -1;
	}
	return 0;
}

static void DestroyVideoProcessor()
{
#ifdef DRAGON_MULTIPLE_ENCODE
	return;
#else
	m_videoCapture->DestroyVideoCapture();
	m_videoCapture.reset();
#endif
}

static int AcquireFrame(PDXGIFRAME frame, bool withCursor,int* cursorType)
{
	return m_videoCapture->AcquireFrame(frame, withCursor, cursorType);
}

static void RegisterVideoCallbacks(PVIDEOCALLBACKS callbacks, void* userData)
{
	m_videoCallbacks = callbacks;
	m_userData = userData;
}

void * InitDragonVideo(PDRAGONVIDEOAPIS apis)
{
	if (apis->version < 0)
	{
		return nullptr;
	}
	apis->RegisterCallbacks = RegisterVideoCallbacks;
	apis->CreateVideoProcessor = CreateVideoProcessor;
	apis->ResetVideoProcessor = ResetVideoProcessor;
	apis->DestroyVideoProcessor = DestroyVideoProcessor;
	apis->AcquireFrame = AcquireFrame;

	return &m_currentVersion;
}

void UninitDragonVideo(void * instance)
{
}
