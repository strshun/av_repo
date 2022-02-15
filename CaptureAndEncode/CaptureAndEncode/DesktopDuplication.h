#ifndef _DRAGON_DESKTOPDUPLICATION_F866DD87_E806_4EA3_9523_BB7D08734966_H_
#define _DRAGON_DESKTOPDUPLICATION_F866DD87_E806_4EA3_9523_BB7D08734966_H_

#include <memory>
#include "FrameData.h"

typedef HRESULT(WINAPI *LPFNCreateDXGIFactory1)(REFIID riid, _COM_Outptr_ void **ppFactory);

namespace Dragon {

	enum CursorType {
		INVALID_CURSOR = 0,		// error
		PNG_CURSOR = 1,				//png光标
		MASK_CURSOR = 2,
		HIDE_CURSOR = 3,			//光标隐藏
		TRANSPARENT_CRUSOR = 4,		//透明光标
		SAME_CURSOR = 5,		// do not change
		DXGI_CURSOR = 6,
		INPUT_CURSOR = 7,		//输入光标事件
        NORMAL_CURSOR = 9,
        BMP_CURSOR = 100,
	};


	class DesktopDuplication {
	public:
		DesktopDuplication();
		~DesktopDuplication();

		static std::unique_ptr<DesktopDuplication> Create();
		HRESULT DuplicateDesktop(FrameData* frameData);
		
		ID3D11Device* GetD3DDevice();
		ID3D11DeviceContext* GetD3DDeviceContext();
		IDXGIOutput* GetDXGIOutput();

		ID3D11Texture2D* CaptureDCDesktop();
		void GetDesc(DXGI_OUTPUT_DESC* desc);

		bool InitWin32Cursor();
		void CaptureWin32Cursor(uint32_t** outbuffer, int& ptrWidth, int& ptrHeight, int& ptrLeft, int& ptrTop, int& type);

	protected:
		bool CreateD3DContext();

	private:

		int m_metaDataSize;
		unsigned char* m_metaDataBuffer;

		HMODULE m_hD3D11;
		HMODULE m_hDXGI;

		PFN_D3D11_CREATE_DEVICE m_lpfnD3D11CreateDevice;
		LPFNCreateDXGIFactory1 m_lpfnCreateDXGIFactory1;

		Microsoft::WRL::ComPtr<ID3D11Device> m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3dContext;

		Microsoft::WRL::ComPtr<IDXGIDevice> m_dxgiDevice;
		Microsoft::WRL::ComPtr<IDXGIFactory1> m_dxgiFactory;
		Microsoft::WRL::ComPtr<IDXGIAdapter> m_dxgiAdapter;
		Microsoft::WRL::ComPtr<IDXGIOutput> m_dxgiOutput;
		Microsoft::WRL::ComPtr<IDXGIOutput1> m_dxgiOutput1;
		Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;

		DXGI_OUTPUT_DESC m_outputDesc;

		//win32 cursor
		HDC m_desktopDc = nullptr;
		uint32_t* m_maskData = nullptr;
		uint32_t* m_imageData = nullptr;
		const int kBytesPerPixel = 4;
	};
}

#endif