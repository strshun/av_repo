#include "DesktopDuplication.h"

#define MAX_CURSOR_BUFFER		(1024 * 1024)
namespace Dragon {


	DesktopDuplication::DesktopDuplication()
		:m_metaDataBuffer(nullptr)
	{
	}

	DesktopDuplication::~DesktopDuplication()
	{
		FreeLibrary(m_hD3D11);
		FreeLibrary(m_hDXGI);

		m_duplication.Reset();
		m_dxgiOutput1.Reset();
		m_dxgiOutput.Reset();
		m_dxgiAdapter.Reset();
		m_dxgiFactory.Reset();
		m_dxgiDevice.Reset();

		m_d3dContext.Reset();
		m_d3dDevice.Reset();

		if (m_maskData)
		{
			delete[] m_maskData;
			m_maskData = nullptr;
		}
		if (m_imageData)
		{
			delete[] m_imageData;
			m_imageData = nullptr;
		}
	}

	std::unique_ptr<DesktopDuplication> DesktopDuplication::Create()
	{
		std::unique_ptr<DesktopDuplication> dup(new DesktopDuplication());
		if (!dup->CreateD3DContext())
		{
			return nullptr;
		}
		return dup;
	}

	bool DesktopDuplication::CreateD3DContext()
	{
		m_hD3D11 = LoadLibraryW(L"d3d11.dll");
		if (!m_hD3D11)
		{
			MessageBox(NULL, L"can not find \"d3d11.dll\"", L"Error", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
			return false;
		}
		m_hDXGI = LoadLibraryW(L"dxgi.dll");
		if (!m_hDXGI)
		{
			MessageBox(NULL, L"can not find \"dxgi.dll\"", L"Error", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
			return false;
		}
		m_lpfnD3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(m_hD3D11, "D3D11CreateDevice");
		if (!m_lpfnD3D11CreateDevice)
		{
			return false;
		}
		m_lpfnCreateDXGIFactory1 = (LPFNCreateDXGIFactory1)GetProcAddress(m_hDXGI, "CreateDXGIFactory1");
		if (!m_lpfnCreateDXGIFactory1)
		{
			return false;
		}

		HRESULT hr = S_OK;
		hr = m_lpfnCreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)m_dxgiFactory.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_dxgiFactory->EnumAdapters(0, m_dxgiAdapter.GetAddressOf());
		if (FAILED(hr)) { return false; }
		
		int createFlag = 0;
#if _DEBUG
		createFlag |= D3D11_CREATE_DEVICE_DEBUG;
#endif
		hr = m_lpfnD3D11CreateDevice(m_dxgiAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, createFlag,
			NULL, 0, D3D11_SDK_VERSION, m_d3dDevice.GetAddressOf(), NULL,
			m_d3dContext.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)m_dxgiDevice.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_dxgiAdapter->EnumOutputs(0, m_dxgiOutput.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_dxgiOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)m_dxgiOutput1.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), m_duplication.GetAddressOf());
		if (FAILED(hr) || hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			return false;
		}

		hr = m_dxgiOutput->GetDesc(&m_outputDesc);
		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	HRESULT DesktopDuplication::DuplicateDesktop(FrameData* frameData)
	{
		HRESULT hr = S_OK;
		if (!m_duplication.Get())
		{
			hr = m_dxgiOutput1->DuplicateOutput(m_d3dDevice.Get(), m_duplication.GetAddressOf());
			printf("can not create dxgi dup : 0x%x \n", hr);
			return S_FALSE;
		}

		hr = m_duplication->ReleaseFrame();
		if (DXGI_ERROR_ACCESS_LOST == hr)
		{
			m_duplication.Reset();
			return hr;
		}

		ID3D11Texture2D* hAcquiredDesktopImage;
		Microsoft::WRL::ComPtr<IDXGIResource> desktopResource;
		DXGI_OUTDUPL_FRAME_INFO frameInfo = { 0 };
		
		hr = m_duplication->AcquireNextFrame(0, &frameInfo, desktopResource.GetAddressOf());
		if (FAILED(hr) && hr != DXGI_ERROR_WAIT_TIMEOUT)
		{
			printf("duplication error 0x%x \n", hr);
			m_duplication.Reset();
			return hr;
		}
		if (frameInfo.LastPresentTime.QuadPart == 0)
		{
			m_duplication->ReleaseFrame();
			return S_FALSE;
		}

		hr = desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&hAcquiredDesktopImage);
		if (FAILED(hr)) { return hr; }

		if (frameInfo.TotalMetadataBufferSize)
		{
			if (frameInfo.TotalMetadataBufferSize > (UINT)m_metaDataSize)
			{
				if (m_metaDataBuffer)
				{
					delete[]m_metaDataBuffer;
					m_metaDataBuffer = nullptr;
				}
				m_metaDataBuffer = new unsigned char[frameInfo.TotalMetadataBufferSize];
				m_metaDataSize = frameInfo.TotalMetadataBufferSize;
			}

			UINT bufSize = frameInfo.TotalMetadataBufferSize;
			hr = m_duplication->GetFrameMoveRects(bufSize, (DXGI_OUTDUPL_MOVE_RECT*)m_metaDataBuffer, &bufSize);
			if (FAILED(hr))
			{
				hAcquiredDesktopImage->Release();
				return hr;
			}

			int moveCount = bufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

			unsigned char* dirtyRects = m_metaDataBuffer + bufSize;
			bufSize = frameInfo.TotalMetadataBufferSize - bufSize;

			hr = m_duplication->GetFrameDirtyRects(bufSize, (RECT*)dirtyRects, &bufSize);
			if (FAILED(hr))
			{
				hAcquiredDesktopImage->Release();
				return hr;
			}
			int dirtyCount = bufSize / sizeof(RECT);

			frameData->dirtyCount = dirtyCount;
			frameData->dirtyRect = dirtyRects;
			frameData->moveCount = moveCount;
			frameData->moveRect = m_metaDataBuffer;
		}

		static bool firstFrame = true;
		if (firstFrame)
		{
			m_d3dContext->CopyResource(hAcquiredDesktopImage, CaptureDCDesktop());
			firstFrame = false;
		}
		frameData->capturedTex = hAcquiredDesktopImage;
		frameData->frameInfo = frameInfo;
		
		return S_OK;
	}

	ID3D11Device * DesktopDuplication::GetD3DDevice()
	{
		return m_d3dDevice.Get();
	}

	ID3D11DeviceContext * DesktopDuplication::GetD3DDeviceContext()
	{
		return m_d3dContext.Get();
	}

	IDXGIOutput * DesktopDuplication::GetDXGIOutput()
	{
		return m_dxgiOutput.Get();
	}

	ID3D11Texture2D * DesktopDuplication::CaptureDCDesktop()
	{
		ID3D11Texture2D* firstDesktop = nullptr;
		int screenWidth = GetSystemMetrics(SM_CXSCREEN);
		int screenHeight = GetSystemMetrics(SM_CYSCREEN);

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = screenWidth;
		desc.Height = screenHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &firstDesktop);
		if (hr != S_OK)
		{
			return nullptr;
		}

		HDC     hDC;
		HDC     memDC;
		BYTE*   pBuffer;
		HBITMAP   hBmp;
		BITMAPINFO   bmpInfo;

		memset(&bmpInfo, 0, sizeof(bmpInfo));
		bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFO);
		bmpInfo.bmiHeader.biWidth = screenWidth;
		bmpInfo.bmiHeader.biHeight = screenHeight;
		bmpInfo.bmiHeader.biPlanes = 1;
		bmpInfo.bmiHeader.biBitCount = 32;

		hDC = GetDC(NULL);
		if (!hDC)
		{
			printf("can not get hdc:%d. ", GetLastError());
			return nullptr;
		}
		memDC = CreateCompatibleDC(hDC);
		if (!memDC)
		{
			printf("can not get memhdc:%d. ", GetLastError());
			return nullptr;
		}
		hBmp = CreateDIBSection(memDC, &bmpInfo, DIB_RGB_COLORS, (void**)&pBuffer, NULL, 0);
		if (!hBmp)
		{
			printf("can not get bmp:%d. ", GetLastError());
			return nullptr;
		}
		HGDIOBJ obj = SelectObject(memDC, hBmp);
		if (!obj)
		{
			printf("can not get obj:%d. ", GetLastError());
			return nullptr;
		}
		BOOL bRet = BitBlt(memDC, 0, 0, bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight, hDC, 0, 0, SRCCOPY);
		if (bRet == FALSE)
		{
			printf("can not get BitBlt:%d. ", GetLastError());
			return nullptr;
		}

		D3D11_MAPPED_SUBRESOURCE mapRes;
		hr = m_d3dContext->Map(firstDesktop, 0, D3D11_MAP_READ, 0, &mapRes);
		if (FAILED(hr))
		{
			printf("can not map texture. ");
			return nullptr;
		}

		for (int i = 0; i < screenHeight; i++)
		{
			int srcLine = i;
			int destLine = screenHeight - i - 1;
			memcpy((unsigned char*)mapRes.pData + destLine * mapRes.RowPitch,
				pBuffer + srcLine * screenWidth * 4, screenWidth * 4);
		}
		m_d3dContext->Unmap(firstDesktop, 0);

		ReleaseDC(NULL, hDC);
		DeleteDC(memDC);
		DeleteObject(hBmp);

		printf("get dc screen scuccess. \n");
		return firstDesktop;
	}
	void DesktopDuplication::GetDesc(DXGI_OUTPUT_DESC* desc)
	{
		memcpy(desc, &m_outputDesc, sizeof(DXGI_OUTDUPL_DESC));
	}
	bool DesktopDuplication::InitWin32Cursor()
	{
		m_imageData = new uint32_t[MAX_CURSOR_BUFFER]();
		m_maskData = new uint32_t[MAX_CURSOR_BUFFER]();
		return true;
	}

	//I型光标加白色边框
	static void AddCursorOutline(int width, int height, uint32_t* data)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				RGBQUAD* rgbPlane = (RGBQUAD*)data;
				//if (*data == 0x00FFFFFF)
				if (rgbPlane->rgbReserved == 0x00)
				{
					if ((y > 0 && data[-width] == 0xFF000000) ||
						(y < height - 1 && data[width] == 0xFF000000) ||
						(x > 0 && data[-1] == 0xFF000000) ||
						(x < width - 1 && data[1] == 0xFF000000)) {
						*data = 0xFFFFFFFF;
					}
				}
				data++;
			}
		}
	}

	void DesktopDuplication::CaptureWin32Cursor(uint32_t** outbuffer, int& ptrWidth, int& ptrHeight, int& ptrLeft, int& ptrTop, int& type)
	{
		if (!m_maskData || !m_imageData)
		{
			InitWin32Cursor();
		}

		CURSORINFO cursorInfo;
		cursorInfo.cbSize = sizeof(CURSORINFO);
		BOOL ret = GetCursorInfo(&cursorInfo);
		if (!ret)
		{
			type = CursorType::INVALID_CURSOR;
			return;
		}
		if (cursorInfo.hCursor == nullptr)
		{
			type = CursorType::HIDE_CURSOR;
			return;
		}
		ICONINFOEXW iconInfo = { 0 };
		iconInfo.cbSize = sizeof(ICONINFOEXW);
		ret = GetIconInfoExW(cursorInfo.hCursor, &iconInfo);
		if (!ret)
		{
			type = CursorType::INVALID_CURSOR;
			return;
		}

		int xHotspot = iconInfo.xHotspot;
		int yHotspot = iconInfo.yHotspot;
		bool isColor = (iconInfo.hbmColor != NULL);

		HBITMAP hMask = iconInfo.hbmMask;
		HBITMAP hColor = iconInfo.hbmColor;

		BITMAP bmpMask;
		ret = GetObject(hMask, sizeof(bmpMask), &bmpMask);
		if (ret == 0)
		{
			type = CursorType::INVALID_CURSOR;
			return;
		}
		int width = bmpMask.bmWidth;
		int height = bmpMask.bmHeight;

		BITMAPV5HEADER bmi = { 0 };
		bmi.bV5Size = sizeof(bmi);
		bmi.bV5Width = width;
		bmi.bV5Height = -height;
		bmi.bV5Planes = 1;
		bmi.bV5BitCount = kBytesPerPixel * 8;
		bmi.bV5Compression = BI_RGB;
		bmi.bV5AlphaMask = 0xff000000;
		bmi.bV5CSType = LCS_WINDOWS_COLOR_SPACE;
		bmi.bV5Intent = LCS_GM_BUSINESS;

		HDC hdc = GetDC(NULL);
		if (!hdc)
		{
			type = CursorType::INVALID_CURSOR;
			return;
		}
		memset(m_maskData, 0x0, MAX_CURSOR_BUFFER * sizeof(uint32_t));
		ret = GetDIBits(hdc, hMask, 0, height, m_maskData, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS);
		if (ret == 0)
		{
			type = CursorType::INVALID_CURSOR;
			ReleaseDC(nullptr, hdc);
			return;
		}
		bool isTransparent = true;

		if (isColor)
		{
			memset(m_imageData, 0x0, MAX_CURSOR_BUFFER * sizeof(uint32_t));
			ret = GetDIBits(hdc, hColor, 0, height, m_imageData, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS);
			if (ret == 0)
			{
				type = CursorType::INVALID_CURSOR;
				ReleaseDC(nullptr, hdc);
				return;
			}
			//彩色无透明通道的图片需要修正透明通道
			bool needModifyAlpha = true;
			for (int i = 0; i < width * height; i++)
			{
				RGBQUAD* rgbPlane = (RGBQUAD*)(m_imageData + i);
				if (rgbPlane->rgbReserved != 0)
				{
					needModifyAlpha = false;
					break;
				}
			}
			for (int i = 0; i < width * height; i++) //alpha修正
			{
				RGBQUAD* rgbPlane = (RGBQUAD*)(m_imageData + i);
				if ((rgbPlane->rgbBlue || rgbPlane->rgbGreen || rgbPlane->rgbRed) //color mask
					&& rgbPlane->rgbReserved == 0x00)
				{
					if (needModifyAlpha)
					{
						rgbPlane->rgbReserved = 0xFF;
					}
				}
				if (rgbPlane->rgbReserved != 0x00)
				{
					isTransparent = false;
				}
			}
		}
		else
		{
			height /= 2;
			memcpy(m_imageData, m_maskData + width * height, 4 * width * height);
			bool needOutline = false;
			for (int i = 0; i < width * height; i++)
			{
				DWORD* mask = (DWORD*)(m_maskData + i);
				DWORD* plane = (DWORD*)(m_imageData + i);
				RGBQUAD* rgbPlane = (RGBQUAD*)plane;
				// The two bitmaps combine as follows:
				//  mask  color   Windows Result   Our result    RGB   Alpha
				//   0     00      Black            Black         00    ff
				//   0     ff      White            White         ff    ff
				//   1     00      Screen           Transparent   00    00
				//   1     ff      Reverse-screen   Black         00    ff
				if (*mask != 0)//mask == 0x00FFFFFF
				{
					if (*plane != 0)
					{
						needOutline = true;
						*plane = 0xFF000000;
					}
					else
					{
						*plane = 0;
					}
				}
				else//mask == 0x00000000
				{
					rgbPlane->rgbReserved = 0xFF;
				}
				if (rgbPlane->rgbReserved != 0x00)
				{
					isTransparent = false;
				}
			}
			if (needOutline)//不支持xor, 使用白色边框替代
			{
				AddCursorOutline(width, height, m_imageData);
			}
		}
		if (isTransparent)
		{
			type = CursorType::HIDE_CURSOR;
		}
		else
		{
			type = CursorType::BMP_CURSOR;
			ptrWidth = width;
			ptrHeight = height;
			ptrLeft = cursorInfo.ptScreenPos.x - iconInfo.xHotspot;
			ptrTop = cursorInfo.ptScreenPos.y - iconInfo.yHotspot;
			*outbuffer = m_imageData;
		}
		DeleteObject(hMask);
		DeleteObject(hColor);
		ReleaseDC(nullptr, hdc);
	}

}