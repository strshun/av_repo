#include <iostream>
#include "VideoCapture.h"
#include "DragonVideo.h"
int main() {
	std::unique_ptr<Dragon::VideoCapture> m_videoCapture;
	Dragon::VideoCapture::VideoConfig videoConfig;
	videoConfig.bitrate = 3000;
	videoConfig.bitateMode = 1;
	videoConfig.encodeHeight = 1080;
	videoConfig.encodeWidth = 1920;
	videoConfig.maxFps = 20;
	videoConfig.videoForamt = 2;
	m_videoCapture = Dragon::VideoCapture::Create(videoConfig);
	if (!m_videoCapture.get())
	{
		return -1;
	}
	DXGIFRAME m_dxgiFrame;

	m_dxgiFrame.frameBuffer = new unsigned char[4000 * 4000 * 4]();
	while (1) {
		static int i = 0;

		m_dxgiFrame.frameType = i % 40 == 0 ? 3 : 1;
		if (i % 40 == 0) {
			i = 0;
		}
		i++;
		int m_cursorType = 1;

		int err = m_videoCapture->AcquireFrame(&m_dxgiFrame, true, &m_cursorType);
		Sleep(50);
	}
	
	return 0;
}