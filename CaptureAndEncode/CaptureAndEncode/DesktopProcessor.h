#ifndef _DRAGON_DESKTOPPROCESSOR_22EACA30_E9B5_4DDD_8D00_5C9EE91ECE2D_H_
#define _DRAGON_DESKTOPPROCESSOR_22EACA30_E9B5_4DDD_8D00_5C9EE91ECE2D_H_

#include <memory>
#include "FrameData.h"
#include <DirectXMath.h>
#include <Windows.h>

#define BPP         4
#define NUMVERTICES			6

using DirectX::XMFLOAT3;
using DirectX::XMFLOAT2;
using Microsoft::WRL::ComPtr;

struct VERTEX
{
	XMFLOAT3 Pos;
	XMFLOAT2 TexCoord;
};

typedef HRESULT(WINAPI *LPFNCreateDXGIFactory1)(REFIID riid, _COM_Outptr_ void **ppFactory);

namespace Dragon {

	class DesktopProcessor {
	public:
		static std::unique_ptr<DesktopProcessor> Create(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext, IDXGIOutput* dxgiOutput);
		DesktopProcessor();
		DesktopProcessor(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext, IDXGIOutput* dxgiOutput);
		~DesktopProcessor();

		HRESULT CreateStage(int width, int height);
		HRESULT ReCreateStage(int width, int height);

		HRESULT ProcessMonoMask(ID3D11Texture2D* srcTex, bool IsMono, _Inout_ PPOINTERINFO ptrInfo, _Out_ INT* PtrWidth,
			_Out_ INT* PtrHeight, _Out_ INT* PtrLeft, _Out_ INT* PtrTop, _Outptr_result_bytebuffer_(*PtrHeight** PtrWidth* BPP) BYTE** InitBuffer, _Out_ D3D11_BOX* Box);
		//texture and cursor
		HRESULT DrawFrame(FrameData* frame, ID3D11Texture2D* destTex);
		HRESULT DrawFrame(ID3D11Texture2D* srcTex, ID3D11Texture2D* destTex);

		HRESULT DrawCursor(FrameData* frame, PPOINTERINFO PtrInfo, ID3D11Texture2D* destTex);
		HRESULT DrawWin32Cursor(ID3D11Texture2D* destTex,
			unsigned char* buffer, int ptrWidth, int ptrHeight, int ptrLeft, int ptrTop);


		void SaveToBMP(ID3D11Texture2D* src);
	protected:
		bool CreateRenderContext();
		void ReleaseRenderContext();
		void SetViewPort(UINT Width, UINT Height);
		

	private:

		ID3D11Device* m_d3dDevice = nullptr;
		ID3D11DeviceContext* m_d3dContext = nullptr;
		IDXGIOutput* m_dxgiOutput = nullptr;

		HMODULE m_hD3DCompile = nullptr;
		pD3DCompile m_lpfnD3DCompile = nullptr;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_psShader;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vsShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_psSampler;

		DXGI_OUTPUT_DESC m_outputDesc;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stageBuffer;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_stageRTV;

	};

}


#endif
