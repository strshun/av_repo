#ifndef _DRAGON_FRAMEDATA_84583AB0_ACE0_4EBC_82E7_7DCDB2D993C2_H_
#define _DRAGON_FRAMEDATA_84583AB0_ACE0_4EBC_82E7_7DCDB2D993C2_H_

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <comdef.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <time.h>

#define CURSOR_MAX_WIDTH 128

namespace Dragon {
	class DXGICursorData {
	public:
		int dxgiType = 0;
		int width = 0;
		int height = 0;
		int xHotspot = 0;
		int yHotspot = 0;
		int pitch = 0;

		int status = 0;
		unsigned char* buffer = nullptr;
		int size = 0;
		DXGICursorData()
		{
			buffer = new unsigned char[CURSOR_MAX_WIDTH * CURSOR_MAX_WIDTH * 4]();
		}
		~DXGICursorData() = default;

		void Clear()
		{
			dxgiType = 0;
			width = 0;
			height = 0;
			xHotspot = 0;
			yHotspot = 0;
			pitch = 0;
			memset(buffer, 0x0, CURSOR_MAX_WIDTH * CURSOR_MAX_WIDTH * 4);
		}
		unsigned char* GetDataOf()
		{
			return buffer;
		}
	};
	class FrameData {

	public:
		int moveCount;	
		unsigned char* moveRect;  //该数据多帧复用 由duplicator管理
		int dirtyCount;
		unsigned char* dirtyRect;
		ID3D11Texture2D* capturedTex;
		DXGI_OUTDUPL_FRAME_INFO frameInfo;
		LONGLONG cpaturedTime;

		static LONGLONG GetTimestamp()
		{
			time_t clock;
			struct tm tm;
			SYSTEMTIME wtm;
			GetLocalTime(&wtm);
			tm.tm_year = wtm.wYear - 1900;
			tm.tm_mon = wtm.wMonth - 1;
			tm.tm_mday = wtm.wDay;
			tm.tm_hour = wtm.wHour;
			tm.tm_min = wtm.wMinute;
			tm.tm_sec = wtm.wSecond;
			tm.tm_isdst = -1;
			clock = mktime(&tm);
			long long timestamp = clock * 1000000 + wtm.wMilliseconds * 1000;

			return timestamp;
		}

	public:
		FrameData() :
			moveCount(0), moveRect(nullptr), dirtyCount(0), dirtyRect(nullptr), capturedTex(nullptr)
		{

		}
		void Release()
		{
			if (capturedTex)
			{
				capturedTex->Release();
				capturedTex = nullptr;
			}
			moveCount = 0;
			moveRect = nullptr;
			dirtyCount = 0;
			dirtyRect = nullptr;
		}
		void Clear()
		{
			if (capturedTex)
			{
				capturedTex->Release();
				capturedTex = nullptr;
			}
			moveCount = 0;
			moveRect = nullptr;
			dirtyCount = 0;
			dirtyRect = nullptr;
		}
	};


	typedef struct tagPOINTERINFO_
	{
		_Field_size_bytes_(BufferSize) BYTE* PtrShapeBuffer;
		DXGI_OUTDUPL_POINTER_SHAPE_INFO ShapeInfo;
		POINT Position;
		bool Visible;
		UINT BufferSize;
		UINT WhoUpdatedPositionLast;
		LARGE_INTEGER LastTimeStamp;
	} POINTERINFO, * PPOINTERINFO;

}


#endif
