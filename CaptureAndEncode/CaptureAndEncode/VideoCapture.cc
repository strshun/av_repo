#include "VideoCapture.h"
#include <process.h>
#include <time.h>
#include <fstream>
namespace Dragon {
	std::unique_ptr<VideoCapture> VideoCapture::Create(VideoConfig config)
	{
		std::unique_ptr<VideoCapture> capture(new VideoCapture(config));
		if (!capture->Init())
		{
			return nullptr;
		}
		return capture;
	}

	VideoCapture::VideoCapture(VideoConfig config)
		: m_config(config)
	{

	}

	VideoCapture::~VideoCapture()
	{
	}

	int VideoCapture::AcquireFrame(PDXGIFRAME frame, bool withCursor, int* cursorType)
	{
		
		int err = 0;
		HRESULT hr = S_OK;
		m_capedFrame.Clear();
		frame->capStartTime = clock();
		hr = m_duplication->DuplicateDesktop(&m_capedFrame);
		//std::ofstream out;
		//out.open("fps.txt", std::ios::app);
		//static clock_t lastFrame = clock();
		//static int count = 0;

		//clock_t curentFrame = clock();
		//static int count1 = 0;
		//if (out.is_open()) {
		//	count++;
		//	//out << "DuplicateDesktop index:" << count++ << "\n";
		//	if (curentFrame - lastFrame > 1000) {
		//		out << "DuplicateDesktop peer second index:"<< count << "\n";
		//		count = 0;
		//	}
		//}
		if (!m_fpsController->AcquireSignal(hr)) //捕获失败或者捕获成功画面没有变化
		{
			//if (out.is_open()) {
			//	count1++;
			//	//out << "AcquireSignal index:"<< count1++ << "\n";
			//	if (curentFrame - lastFrame > 1000) {
			//		lastFrame = clock();
			//		out << "AcquireSignal peer second index:" << count1 << "\n";
			//		count1 = 0;
			//	}
			//}
			return -1;
		}
		else
		{
			/*if (curentFrame - lastFrame > 1000) {
				lastFrame = clock();
				out << "AcquireSignal peer second index:" << count1 << "\n";
				count1 = 0;
			}*/
			unsigned char* outBuffer = nullptr;
			int type = 0;
			int ptrWidth = 0, ptrHeight = 0, ptrLeft = 0, ptrTop = 0;
			if (withCursor)
			{
				m_duplication->CaptureWin32Cursor((uint32_t**)&outBuffer, ptrWidth, ptrHeight, ptrLeft, ptrTop, type);
			}
			if (m_capedFrame.capturedTex && hr == S_OK)
			{
				D3D11_TEXTURE2D_DESC desc;
				m_capedFrame.capturedTex->GetDesc(&desc);
				if (m_config.encodeWidth == desc.Width && m_config.encodeHeight == desc.Height && !withCursor)
				{
					std::lock_guard<std::mutex> guard(m_d3dMutex);
					m_d3d11DeviceContext->CopyResource(m_renderStage.Get(), m_capedFrame.capturedTex);
					m_d3d11DeviceContext->CopyResource(m_lastStage.Get(), m_capedFrame.capturedTex);
				}
				else
				{
					hr = m_renderer->DrawFrame(&m_capedFrame, m_renderStage.Get());
					if (hr != S_OK)
					{
						return -2;
					}
					m_d3d11DeviceContext->CopyResource(m_lastStage.Get(), m_renderStage.Get());
					if(withCursor)
					{
						
						if (type == CursorType::BMP_CURSOR)
						{
							*cursorType = CursorType::PNG_CURSOR;
							m_renderer->DrawWin32Cursor(m_renderStage.Get(), outBuffer, ptrWidth, ptrHeight, ptrLeft, ptrTop);
						}
						else
						{
							*cursorType = CursorType::HIDE_CURSOR;
							printf("can not cap cursor %d\n", GetLastError());
						}
					}
				}
			}
			else
			{
				if (withCursor)
				{
					hr = m_renderer->DrawFrame(m_lastStage.Get(), m_renderStage.Get());
					if (hr != S_OK)
					{
						return -2;
					}
					if (type == CursorType::BMP_CURSOR)
					{
						*cursorType = CursorType::PNG_CURSOR;
						m_renderer->DrawWin32Cursor(m_renderStage.Get(), outBuffer, ptrWidth, ptrHeight, ptrLeft, ptrTop);
					}
					else if(type == CursorType::HIDE_CURSOR)
					{
						*cursorType = CursorType::HIDE_CURSOR;
					}
					else
					{
						printf("can not cap cursor %d\n", GetLastError());
					}
				}
			}
		}

		frame->capEndTime = clock();

		m_encdFrame.srcTexture = m_renderStage.Get();
		m_encdFrame.ClearBuffer();
		bool keyFrame = false;
		if (frame->frameType == 3)
		{
			keyFrame = true;
		}
		clock_t startEncodeTime = clock();
		err = m_videoEncoder->DoEncode(m_encdFrame, keyFrame);
		clock_t endEncodeTime = clock();
		/*if ((endEncodeTime - startEncodeTime) > 15) {
			out << "\t end encode time: " << timer.now() << "ClockTime" << (endEncodeTime - startEncodeTime) << "\t err: " << err << "\n";
		}
		
		out.close();*/
		if (err == 0)
		{
			frame->encEndTime = clock();
			frame->frameType = m_encdFrame.m_frameType;
			memcpy(frame->frameBuffer, m_encdFrame.m_pBuffer, m_encdFrame.m_length);
			frame->frameSize = m_encdFrame.m_length;
			return 0;
		}

		return -2;
	}

	bool VideoCapture::ReconfigVideoCapture(VideoConfig config)
	{
		VideoEncoder::VideoEncoderConfig encConfig;
		encConfig.bitrate = config.bitrate;
		encConfig.bitrateMode = config.bitateMode;
		encConfig.clientType = config.clientType;
		encConfig.encodeHeight = config.encodeHeight;
		encConfig.encodeWidth = config.encodeWidth;
		encConfig.extraOutputDelay = config.outputDelay;
		encConfig.hevc = (config.videoForamt == 3) ? true : false;
		encConfig.fps = config.maxFps;

		bool err = m_videoEncoder->Reconfig(encConfig);
		if (!err)
		{
			return false;
		}
		
		HRESULT hr = m_renderer->ReCreateStage(config.encodeWidth, config.encodeHeight);
		if (hr != S_OK)
		{
			return false;
		}

		if (m_renderStage.Get())
		{
			D3D11_TEXTURE2D_DESC desc = { 0 };
			m_renderStage->GetDesc(&desc);
			if (desc.Width == config.encodeWidth && desc.Height == config.encodeHeight)
			{
				memcpy(&m_config, &config, sizeof(VideoConfig));
				return true;
			}
			desc.Width = config.encodeWidth;
			desc.Height = config.encodeHeight;

			std::lock_guard<std::mutex> guard(m_d3dMutex);
			HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, m_renderStage.GetAddressOf());
			if (hr != S_OK)
			{
				return false;
			}

			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, m_lastStage.GetAddressOf());
			if (hr != S_OK)
			{
				return false;
			}

		}
		memcpy(&m_config, &config, sizeof(VideoConfig));
		return true;
	}

	void VideoCapture::DestroyVideoCapture()
	{
		
	}

	bool VideoCapture::Init()
	{
		bool ret = false;

		FpsController::FpsControllerConfig fpsConfig;
		m_fpsController = FpsController::Create(fpsConfig);
		if (!m_fpsController.get())
		{
			return false;
		}

		m_duplication = DesktopDuplication::Create();
		if (!m_duplication)
		{
			return false;
		}
		
		m_d3dDevice = m_duplication->GetD3DDevice();
		m_d3d11DeviceContext = m_duplication->GetD3DDeviceContext();
		m_dxgiOutput = m_duplication->GetDXGIOutput();
		
		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.Width = m_config.encodeWidth;
		desc.Height = m_config.encodeHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, m_renderStage.GetAddressOf());
		if (hr != S_OK)
		{
			return false;
		}
		
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, m_lastStage.GetAddressOf());
		if (hr != S_OK)
		{
			return false;
		}

		m_renderer = Dragon::DesktopProcessor::Create(m_d3dDevice, m_d3d11DeviceContext, m_dxgiOutput);
		if (!m_renderer.get())
		{
			return false;
		}
		hr = m_renderer->CreateStage(m_config.encodeWidth, m_config.encodeHeight);
		if (hr != S_OK)
		{
			return false;
		}

		m_videoEncoder = Dragon::VideoEncoder::Create(NVENC_ENCODER);
		if (!m_videoEncoder)
		{
			return false;
		}
		m_videoEncoder->SetD3DContext(m_d3dDevice, m_d3d11DeviceContext);
		VideoEncoder::VideoEncoderConfig config;
		config.bitrate = m_config.bitrate;
		config.bitrateMode = m_config.bitateMode;
		config.clientType = 1;
		config.encodeHeight = m_config.encodeHeight;
		config.encodeWidth = m_config.encodeWidth;
		config.extraOutputDelay = 10;
		config.hevc = (m_config.videoForamt == 3) ? true : false;
		config.fps = m_config.maxFps;
		ret = m_videoEncoder->Start(config);
		if (!ret)
		{
			return false;
		}

		return true;
	}

}