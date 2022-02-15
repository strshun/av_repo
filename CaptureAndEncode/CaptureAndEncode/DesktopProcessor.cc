#include "DesktopProcessor.h"
#include <DirectXMath.h>

namespace Dragon {

#if 1
	const char D3D11_SCALE_SHADER[] = ""
		"Texture2D tex_source: register(t0);\n"
		"SamplerState sam_linear: register(s0);\n"
		"\n"
		"struct VertexInput {\n"
		"  float4 pos: POSITION;\n"
		"  float2 tex: TEXCOORD0;\n"
		"};\n"
		"\n"
		"struct PixelInput {\n"
		"  float4 pos: SV_POSITION;\n"
		"  float2 tex: TEXCOORD0;\n"
		"};\n"
		"\n"
		"PixelInput vertex_shader(VertexInput vertex) {\n"
		"  PixelInput output = (PixelInput) 0;\n"
		"  output.pos = vertex.pos;\n"
		"  output.tex = vertex.tex;\n"
		"  return output;\n"
		"}\n"
		"\n"
		"float4 pixel_shader(PixelInput input) : SV_Target {\n"
		"  float4 pix = tex_source.Sample(sam_linear, input.tex);\n"
//		"  pix.a = 1.0;\n"
		"  return pix;\n;"
		"}\n";
#endif

	std::unique_ptr<DesktopProcessor> DesktopProcessor::Create(ID3D11Device * d3dDevice, ID3D11DeviceContext * d3dContext, IDXGIOutput* dxgiOutput)
	{
		std::unique_ptr<DesktopProcessor> processor(new DesktopProcessor(d3dDevice, d3dContext, dxgiOutput));
		if (!processor->CreateRenderContext())
		{
			return nullptr;
		}
		return processor;
	}
	DesktopProcessor::DesktopProcessor()
	{
	}

	DesktopProcessor::DesktopProcessor(ID3D11Device* d3dDevice, ID3D11DeviceContext * d3dContext, IDXGIOutput* dxgiOutput)
		: m_d3dDevice(d3dDevice), m_d3dContext(d3dContext), m_dxgiOutput(dxgiOutput)
	{
	}

	DesktopProcessor::~DesktopProcessor()
	{
	}

	HRESULT DesktopProcessor::CreateStage(int width, int height)
	{
		HRESULT hr = S_OK;
		D3D11_TEXTURE2D_DESC desc = { 0 };
		
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, m_stageBuffer.GetAddressOf());
		if (hr != S_OK)
		{
			return hr;
		}
		
		hr = m_d3dDevice->CreateRenderTargetView(m_stageBuffer.Get(), nullptr, m_stageRTV.GetAddressOf());
		if (hr != S_OK)
		{
			return hr;
		}
		printf("create renderstage %d*%d \n", width, height);

		SetViewPort(width, height);

		return S_OK;
	}

	HRESULT DesktopProcessor::ReCreateStage(int width, int height)
	{
		D3D11_TEXTURE2D_DESC desc = { 0 };
		m_stageBuffer->GetDesc(&desc);
		if (desc.Width != width || desc.Height != height)
		{
			m_stageBuffer.Reset();
			m_stageRTV.Reset();
			return CreateStage(width, height);
		}
		return S_OK;
	}

	bool DesktopProcessor::CreateRenderContext()
	{
		HRESULT hr = S_OK;

		m_hD3DCompile = LoadLibraryW(L"D3dcompiler_47.dll");
		if (!m_hD3DCompile)
		{
			MessageBox(NULL, L"can not find \"D3dcompiler_47.dll\"", L"Error", MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
			return false;
		}
		m_lpfnD3DCompile = (pD3DCompile)GetProcAddress(m_hD3DCompile, "D3DCompile");
		if (!m_lpfnD3DCompile)
		{
			return false;
		}

		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> vsErr;
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psErr;

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

		//complie shader and create vs & ps
		hr = m_lpfnD3DCompile(D3D11_SCALE_SHADER, strlen(D3D11_SCALE_SHADER), NULL, NULL, NULL,
			"vertex_shader", "vs_4_0", flags, 0, vsBlob.GetAddressOf(), vsErr.GetAddressOf());
		if (FAILED(hr)) { return false; }
		hr = m_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
			NULL, m_vsShader.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_lpfnD3DCompile(D3D11_SCALE_SHADER, strlen(D3D11_SCALE_SHADER), NULL, NULL, NULL,
			"pixel_shader", "ps_4_0", flags, 0, psBlob.GetAddressOf(), psErr.GetAddressOf());
		if (FAILED(hr)) { return false; }

		hr = m_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
			NULL, m_psShader.GetAddressOf());
		if (FAILED(hr)) { return false; }

		//create vertex input layout
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,	D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,	 0, 12,	D3D11_INPUT_PER_VERTEX_DATA, 0}
		};
		hr = m_d3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(), m_inputLayout.GetAddressOf());
		if (FAILED(hr)) { return false; }

		//create blend state
		D3D11_BLEND_DESC desc = { 0 };
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		hr = m_d3dDevice->CreateBlendState(&desc, m_blendState.GetAddressOf());
		if (FAILED(hr)) { return false; }

		//create sampler state
		D3D11_SAMPLER_DESC samplerDesc;
		ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));

		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		hr = m_d3dDevice->CreateSamplerState(&samplerDesc, m_psSampler.GetAddressOf());
		if (FAILED(hr)) { return false; }

		m_dxgiOutput->GetDesc(&m_outputDesc);

		return true;
	}

	void DesktopProcessor::ReleaseRenderContext()
	{
#if 0
		m_psSampler.Reset();
		m_blendState.Reset();
		m_inputLayout.Reset();
		m_vsShader.Reset();
		m_psShader.Reset();
#endif
	}

	void DesktopProcessor::SetViewPort(UINT Width, UINT Height)
	{
		D3D11_VIEWPORT VP;
		VP.Width = static_cast<FLOAT>(Width);
		VP.Height = static_cast<FLOAT>(Height);
		VP.MinDepth = 0.0f;
		VP.MaxDepth = 1.0f;
		VP.TopLeftX = 0;
		VP.TopLeftY = 0;
		m_d3dContext->RSSetViewports(1, &VP);
	}

	HRESULT DesktopProcessor::ProcessMonoMask(ID3D11Texture2D* srcTex, bool IsMono, PPOINTERINFO PtrInfo, INT* PtrWidth,
		INT* PtrHeight, INT* PtrLeft, INT* PtrTop, BYTE** InitBuffer, D3D11_BOX* Box)
	{
		// Desktop dimensions
		D3D11_TEXTURE2D_DESC FullDesc;
		srcTex->GetDesc(&FullDesc);
		INT DesktopWidth = FullDesc.Width;
		INT DesktopHeight = FullDesc.Height;

		// Pointer position
		INT GivenLeft = PtrInfo->Position.x;
		INT GivenTop = PtrInfo->Position.y;

		// Figure out if any adjustment is needed for out of bound positions
		if (GivenLeft < 0)
		{
			*PtrWidth = GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width);
		}
		else if ((GivenLeft + static_cast<INT>(PtrInfo->ShapeInfo.Width)) > DesktopWidth)
		{
			*PtrWidth = DesktopWidth - GivenLeft;
		}
		else
		{
			*PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);
		}

		if (IsMono)
		{
			PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height / 2;
		}

		if (GivenTop < 0)
		{
			*PtrHeight = GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height);
		}
		else if ((GivenTop + static_cast<INT>(PtrInfo->ShapeInfo.Height)) > DesktopHeight)
		{
			*PtrHeight = DesktopHeight - GivenTop;
		}
		else
		{
			*PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);
		}

		if (IsMono)
		{
			PtrInfo->ShapeInfo.Height = PtrInfo->ShapeInfo.Height * 2;
		}

		*PtrLeft = (GivenLeft < 0) ? 0 : GivenLeft;
		*PtrTop = (GivenTop < 0) ? 0 : GivenTop;

		// Staging buffer/texture
		D3D11_TEXTURE2D_DESC CopyBufferDesc;
		CopyBufferDesc.Width = *PtrWidth;
		CopyBufferDesc.Height = *PtrHeight;
		CopyBufferDesc.MipLevels = 1;
		CopyBufferDesc.ArraySize = 1;
		CopyBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		CopyBufferDesc.SampleDesc.Count = 1;
		CopyBufferDesc.SampleDesc.Quality = 0;
		CopyBufferDesc.Usage = D3D11_USAGE_STAGING;
		CopyBufferDesc.BindFlags = 0;
		CopyBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		CopyBufferDesc.MiscFlags = 0;

		ID3D11Texture2D* CopyBuffer = nullptr;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&CopyBufferDesc, nullptr, &CopyBuffer);
		if (FAILED(hr))
		{
			return S_FALSE;
		}

		// Copy needed part of desktop image
		Box->left = *PtrLeft;
		Box->top = *PtrTop;
		Box->right = *PtrLeft + *PtrWidth;
		Box->bottom = *PtrTop + *PtrHeight;
		m_d3dContext->CopySubresourceRegion(CopyBuffer, 0, 0, 0, 0, srcTex, 0, Box);

		// QI for IDXGISurface
		IDXGISurface* CopySurface = nullptr;
		hr = CopyBuffer->QueryInterface(__uuidof(IDXGISurface), (void**)&CopySurface);
		CopyBuffer->Release();
		CopyBuffer = nullptr;
		if (FAILED(hr))
		{
			return hr;
		}

		// Map pixels
		DXGI_MAPPED_RECT MappedSurface;
		hr = CopySurface->Map(&MappedSurface, DXGI_MAP_READ);
		if (FAILED(hr))
		{
			CopySurface->Release();
			CopySurface = nullptr;
			return hr;
		}

		// New mouseshape buffer
		*InitBuffer = new (std::nothrow) BYTE[*PtrWidth * *PtrHeight * BPP];
		if (!(*InitBuffer))
		{
			return S_FALSE;
		}

		UINT* InitBuffer32 = reinterpret_cast<UINT*>(*InitBuffer);
		UINT* Desktop32 = reinterpret_cast<UINT*>(MappedSurface.pBits);
		UINT  DesktopPitchInPixels = MappedSurface.Pitch / sizeof(UINT);

		// What to skip (pixel offset)
		UINT SkipX = (GivenLeft < 0) ? (-1 * GivenLeft) : (0);
		UINT SkipY = (GivenTop < 0) ? (-1 * GivenTop) : (0);

		if (IsMono)
		{
			for (INT Row = 0; Row < *PtrHeight; ++Row)
			{
				// Set mask
				BYTE Mask = 0x80;
				Mask = Mask >> (SkipX % 8);
				for (INT Col = 0; Col < *PtrWidth; ++Col)
				{
					// Get masks using appropriate offsets
					BYTE AndMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
					BYTE XorMask = PtrInfo->PtrShapeBuffer[((Col + SkipX) / 8) + ((Row + SkipY + (PtrInfo->ShapeInfo.Height / 2)) * (PtrInfo->ShapeInfo.Pitch))] & Mask;
					UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
					UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

					// Set new pixel
					InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] & AndMask32) ^ XorMask32;

					// Adjust mask
					if (Mask == 0x01)
					{
						Mask = 0x80;
					}
					else
					{
						Mask = Mask >> 1;
					}
				}
			}
		}
		else
		{
			UINT* Buffer32 = reinterpret_cast<UINT*>(PtrInfo->PtrShapeBuffer);

			// Iterate through pixels
			for (INT Row = 0; Row < *PtrHeight; ++Row)
			{
				for (INT Col = 0; Col < *PtrWidth; ++Col)
				{
					// Set up mask
					UINT MaskVal = 0xFF000000 & Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))];
					if (MaskVal)
					{
						// Mask was 0xFF
						InitBuffer32[(Row * *PtrWidth) + Col] = (Desktop32[(Row * DesktopPitchInPixels) + Col] ^ Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))]) | 0xFF000000;
					}
					else
					{
						// Mask was 0x00
						InitBuffer32[(Row * *PtrWidth) + Col] = Buffer32[(Col + SkipX) + ((Row + SkipY) * (PtrInfo->ShapeInfo.Pitch / sizeof(UINT)))] | 0xFF000000;
					}
				}
			}
		}

		// Done with resource
		hr = CopySurface->Unmap();
		CopySurface->Release();
		CopySurface = nullptr;
		if (FAILED(hr))
		{
			return hr;
		}

		return S_OK;
	}

	HRESULT DesktopProcessor::DrawFrame(FrameData* frame, ID3D11Texture2D* destTex)
	{
		HRESULT hr = S_OK;
		D3D11_TEXTURE2D_DESC FrameDesc;
		frame->capturedTex->GetDesc(&FrameDesc);

		VERTEX Vertices[NUMVERTICES] =
		{
			{XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, 1.0f, 0), XMFLOAT2(1.0f, 0.0f)},
		};

		D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
		ShaderDesc.Format = FrameDesc.Format;
		ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		ShaderDesc.Texture2D.MostDetailedMip = FrameDesc.MipLevels - 1;
		ShaderDesc.Texture2D.MipLevels = FrameDesc.MipLevels;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResource;
		hr = m_d3dDevice->CreateShaderResourceView(frame->capturedTex, &ShaderDesc, ShaderResource.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		UINT Stride = sizeof(VERTEX);
		UINT Offset = 0;
		FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		m_d3dContext->IASetInputLayout(m_inputLayout.Get());
		m_d3dContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
		m_d3dContext->OMSetRenderTargets(1, m_stageRTV.GetAddressOf(), nullptr);
		m_d3dContext->VSSetShader(m_vsShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShader(m_psShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShaderResources(0, 1, ShaderResource.GetAddressOf());
		m_d3dContext->PSSetSamplers(0, 1, m_psSampler.GetAddressOf());
		m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D11_BUFFER_DESC BufferDesc;
		RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
		BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDesc.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		RtlZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = Vertices;

		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
		hr = m_d3dDevice->CreateBuffer(&BufferDesc, &InitData, VertexBuffer.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}
		m_d3dContext->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &Stride, &Offset);
		m_d3dContext->Draw(NUMVERTICES, 0);

		if (destTex)
		{
			m_d3dContext->CopyResource(destTex, m_stageBuffer.Get());
		}

		return S_OK;
	}

	HRESULT DesktopProcessor::DrawFrame(ID3D11Texture2D* srcTex, ID3D11Texture2D* destTex)
	{
		HRESULT hr = S_OK;
		D3D11_TEXTURE2D_DESC FrameDesc;
		srcTex->GetDesc(&FrameDesc);

		VERTEX Vertices[NUMVERTICES] =
		{
			{XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, 1.0f, 0), XMFLOAT2(1.0f, 0.0f)},
		};

		D3D11_SHADER_RESOURCE_VIEW_DESC ShaderDesc;
		ShaderDesc.Format = FrameDesc.Format;
		ShaderDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		ShaderDesc.Texture2D.MostDetailedMip = FrameDesc.MipLevels - 1;
		ShaderDesc.Texture2D.MipLevels = FrameDesc.MipLevels;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResource;
		hr = m_d3dDevice->CreateShaderResourceView(srcTex, &ShaderDesc, ShaderResource.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		UINT Stride = sizeof(VERTEX);
		UINT Offset = 0;
		FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		m_d3dContext->IASetInputLayout(m_inputLayout.Get());
		m_d3dContext->OMSetBlendState(nullptr, blendFactor, 0xffffffff);
		m_d3dContext->OMSetRenderTargets(1, m_stageRTV.GetAddressOf(), nullptr);
		m_d3dContext->VSSetShader(m_vsShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShader(m_psShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShaderResources(0, 1, ShaderResource.GetAddressOf());
		m_d3dContext->PSSetSamplers(0, 1, m_psSampler.GetAddressOf());
		m_d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D11_BUFFER_DESC BufferDesc;
		RtlZeroMemory(&BufferDesc, sizeof(BufferDesc));
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
		BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BufferDesc.CPUAccessFlags = 0;
		D3D11_SUBRESOURCE_DATA InitData;
		RtlZeroMemory(&InitData, sizeof(InitData));
		InitData.pSysMem = Vertices;

		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
		hr = m_d3dDevice->CreateBuffer(&BufferDesc, &InitData, VertexBuffer.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}
		m_d3dContext->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &Stride, &Offset);
		m_d3dContext->Draw(NUMVERTICES, 0);

		if (destTex)
		{
			m_d3dContext->CopyResource(destTex, m_stageBuffer.Get());
		}

		return S_OK;
	}

	HRESULT DesktopProcessor::DrawCursor(FrameData* frame, PPOINTERINFO PtrInfo, ID3D11Texture2D* destTex)
	{
		D3D11_TEXTURE2D_DESC FullDesc;
		m_stageBuffer->GetDesc(&FullDesc);
		INT DesktopWidth = FullDesc.Width;
		INT DesktopHeight = FullDesc.Height;

		// Center of desktop dimensions
		INT CenterX = (DesktopWidth / 2);
		INT CenterY = (DesktopHeight / 2);

		// Clipping adjusted coordinates / dimensions
		INT PtrWidth = 0;
		INT PtrHeight = 0;
		INT PtrLeft = 0;
		INT PtrTop = 0;

		// Buffer used if necessary (in case of monochrome or masked pointer)
		BYTE* InitBuffer = nullptr;

		// Used for copying pixels
		D3D11_BOX Box;
		Box.front = 0;
		Box.back = 1;

		switch (PtrInfo->ShapeInfo.Type)
		{
		case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
		{
			PtrLeft = PtrInfo->Position.x;
			PtrTop = PtrInfo->Position.y;

			PtrWidth = static_cast<INT>(PtrInfo->ShapeInfo.Width);
			PtrHeight = static_cast<INT>(PtrInfo->ShapeInfo.Height);
			break;
		}

		case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
		{
			ProcessMonoMask(frame->capturedTex, true, PtrInfo, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box);
			break;
		}

		case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
		{
			ProcessMonoMask(frame->capturedTex, false, PtrInfo, &PtrWidth, &PtrHeight, &PtrLeft, &PtrTop, &InitBuffer, &Box);
			break;
		}

		default:
			break;
		}

		VERTEX Vertices[NUMVERTICES] =
		{
			{XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, 1.0f, 0), XMFLOAT2(1.0f, 0.0f)},
		};

		Vertices[0].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
		Vertices[0].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
		Vertices[1].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
		Vertices[1].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;
		Vertices[2].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
		Vertices[2].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
		Vertices[3].Pos.x = Vertices[2].Pos.x;
		Vertices[3].Pos.y = Vertices[2].Pos.y;
		Vertices[4].Pos.x = Vertices[1].Pos.x;
		Vertices[4].Pos.y = Vertices[1].Pos.y;
		Vertices[5].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
		Vertices[5].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;

		D3D11_TEXTURE2D_DESC Desc;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = 0;
		Desc.Width = PtrWidth;
		Desc.Height = PtrHeight;

		D3D11_SHADER_RESOURCE_VIEW_DESC SDesc;
		SDesc.Format = Desc.Format;
		SDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SDesc.Texture2D.MostDetailedMip = Desc.MipLevels - 1;
		SDesc.Texture2D.MipLevels = Desc.MipLevels;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->PtrShapeBuffer : InitBuffer;
		InitData.SysMemPitch = (PtrInfo->ShapeInfo.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR) ? PtrInfo->ShapeInfo.Pitch : PtrWidth * BPP;
		InitData.SysMemSlicePitch = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> MouseTex;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&Desc, &InitData, MouseTex.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderRes;
		hr = m_d3dDevice->CreateShaderResourceView(MouseTex.Get(), &SDesc, ShaderRes.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		D3D11_BUFFER_DESC BDesc;
		ZeroMemory(&BDesc, sizeof(D3D11_BUFFER_DESC));
		BDesc.Usage = D3D11_USAGE_DEFAULT;
		BDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
		BDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BDesc.CPUAccessFlags = 0;

		ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
		InitData.pSysMem = Vertices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBufferMouse;
		hr = m_d3dDevice->CreateBuffer(&BDesc, &InitData, VertexBufferMouse.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		UINT Stride = sizeof(VERTEX);
		UINT Offset = 0;
		m_d3dContext->IASetVertexBuffers(0, 1, VertexBufferMouse.GetAddressOf(), &Stride, &Offset);
		m_d3dContext->OMSetBlendState(m_blendState.Get(), BlendFactor, 0xFFFFFFFF);
		m_d3dContext->OMSetRenderTargets(1, m_stageRTV.GetAddressOf(), nullptr);
		m_d3dContext->VSSetShader(m_vsShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShader(m_psShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShaderResources(0, 1, ShaderRes.GetAddressOf());
		m_d3dContext->PSSetSamplers(0, 1, m_psSampler.GetAddressOf());

		m_d3dContext->Draw(NUMVERTICES, 0);

		//SaveToBMP(m_stageBuffer.Get());
		if (destTex)
		{
			m_d3dContext->CopyResource(destTex, m_stageBuffer.Get());
		}

		if (InitBuffer)
		{
			delete[] InitBuffer;
			InitBuffer = nullptr;
		}
		return S_OK;
	}

	HRESULT DesktopProcessor::DrawWin32Cursor(ID3D11Texture2D* destTex, 
		unsigned char* buffer, int ptrWidth, int ptrHeight, int ptrLeft, int ptrTop)
	{
		D3D11_TEXTURE2D_DESC FullDesc;
		m_stageBuffer->GetDesc(&FullDesc);
		INT DesktopWidth = FullDesc.Width;
		INT DesktopHeight = FullDesc.Height;

		// Center of desktop dimensions
		INT CenterX = (DesktopWidth / 2);
		INT CenterY = (DesktopHeight / 2);

		// Clipping adjusted coordinates / dimensions
		INT PtrWidth = 0;
		INT PtrHeight = 0;
		INT PtrLeft = 0;
		INT PtrTop = 0;

		// Buffer used if necessary (in case of monochrome or masked pointer)
		BYTE* InitBuffer = nullptr;

		// Used for copying pixels
		D3D11_BOX Box;
		Box.front = 0;
		Box.back = 1;

		int currentWidth = GetSystemMetrics(SM_CXSCREEN);
		int currentHeight = GetSystemMetrics(SM_CYSCREEN);
		PtrLeft = ptrLeft * FullDesc.Width / currentWidth;
		PtrTop = ptrTop * FullDesc.Height / currentHeight;

		PtrWidth = ptrWidth;
		PtrHeight = ptrHeight;
		
		VERTEX Vertices[NUMVERTICES] =
		{
			{XMFLOAT3(-1.0f, -1.0f, 0), XMFLOAT2(0.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(1.0f, -1.0f, 0), XMFLOAT2(1.0f, 1.0f)},
			{XMFLOAT3(-1.0f, 1.0f, 0), XMFLOAT2(0.0f, 0.0f)},
			{XMFLOAT3(1.0f, 1.0f, 0), XMFLOAT2(1.0f, 0.0f)},
		};

		Vertices[0].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
		Vertices[0].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
		Vertices[1].Pos.x = (PtrLeft - CenterX) / (FLOAT)CenterX;
		Vertices[1].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;
		Vertices[2].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
		Vertices[2].Pos.y = -1 * ((PtrTop + PtrHeight) - CenterY) / (FLOAT)CenterY;
		Vertices[3].Pos.x = Vertices[2].Pos.x;
		Vertices[3].Pos.y = Vertices[2].Pos.y;
		Vertices[4].Pos.x = Vertices[1].Pos.x;
		Vertices[4].Pos.y = Vertices[1].Pos.y;
		Vertices[5].Pos.x = ((PtrLeft + PtrWidth) - CenterX) / (FLOAT)CenterX;
		Vertices[5].Pos.y = -1 * (PtrTop - CenterY) / (FLOAT)CenterY;

		D3D11_TEXTURE2D_DESC Desc;
		Desc.MipLevels = 1;
		Desc.ArraySize = 1;
		Desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		Desc.SampleDesc.Count = 1;
		Desc.SampleDesc.Quality = 0;
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = 0;
		Desc.MiscFlags = 0;
		Desc.Width = PtrWidth;
		Desc.Height = PtrHeight;

		D3D11_SHADER_RESOURCE_VIEW_DESC SDesc;
		SDesc.Format = Desc.Format;
		SDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SDesc.Texture2D.MostDetailedMip = Desc.MipLevels - 1;
		SDesc.Texture2D.MipLevels = Desc.MipLevels;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = buffer;
		InitData.SysMemPitch = PtrWidth * BPP;
		InitData.SysMemSlicePitch = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> MouseTex;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&Desc, &InitData, MouseTex.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderRes;
		hr = m_d3dDevice->CreateShaderResourceView(MouseTex.Get(), &SDesc, ShaderRes.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		D3D11_BUFFER_DESC BDesc;
		ZeroMemory(&BDesc, sizeof(D3D11_BUFFER_DESC));
		BDesc.Usage = D3D11_USAGE_DEFAULT;
		BDesc.ByteWidth = sizeof(VERTEX) * NUMVERTICES;
		BDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		BDesc.CPUAccessFlags = 0;

		ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
		InitData.pSysMem = Vertices;
		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBufferMouse;
		hr = m_d3dDevice->CreateBuffer(&BDesc, &InitData, VertexBufferMouse.GetAddressOf());
		if (FAILED(hr))
		{
			return hr;
		}

		FLOAT BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		UINT Stride = sizeof(VERTEX);
		UINT Offset = 0;
		m_d3dContext->IASetVertexBuffers(0, 1, VertexBufferMouse.GetAddressOf(), &Stride, &Offset);
		m_d3dContext->OMSetBlendState(m_blendState.Get(), BlendFactor, 0xFFFFFFFF);
		m_d3dContext->OMSetRenderTargets(1, m_stageRTV.GetAddressOf(), nullptr);
		m_d3dContext->VSSetShader(m_vsShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShader(m_psShader.Get(), nullptr, 0);
		m_d3dContext->PSSetShaderResources(0, 1, ShaderRes.GetAddressOf());
		m_d3dContext->PSSetSamplers(0, 1, m_psSampler.GetAddressOf());

		m_d3dContext->Draw(NUMVERTICES, 0);

		//SaveToBMP(m_stageBuffer.Get());
		if (destTex)
		{
			m_d3dContext->CopyResource(destTex, m_stageBuffer.Get());
		}

		if (InitBuffer)
		{
			delete[] InitBuffer;
			InitBuffer = nullptr;
		}
		return S_OK;
	}


	static int SaveBmp(int index, int imageWidth, int imageHeight, unsigned char* pBuffer)
	{
		unsigned char header[54] = {
		0x42, 0x4d, 0, 0, 0, 0, 0, 0, 0, 0,
		54, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 32, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0
		};

		long file_size = (long)imageWidth * (long)imageHeight * 4 + 54;
		header[2] = (unsigned char)(file_size & 0x000000ff);
		header[3] = (file_size >> 8) & 0x000000ff;
		header[4] = (file_size >> 16) & 0x000000ff;
		header[5] = (file_size >> 24) & 0x000000ff;

		long width = imageWidth;
		header[18] = width & 0x000000ff;
		header[19] = (width >> 8) & 0x000000ff;
		header[20] = (width >> 16) & 0x000000ff;
		header[21] = (width >> 24) & 0x000000ff;

		long height = -imageHeight;
		header[22] = height & 0x000000ff;
		header[23] = (height >> 8) & 0x000000ff;
		header[24] = (height >> 16) & 0x000000ff;
		header[25] = (height >> 24) & 0x000000ff;

		char szFilePath[MAX_PATH];
		sprintf_s(szFilePath, sizeof(szFilePath), "pic/pic%d.bmp", index);

		FILE* pFile = NULL;
		fopen_s(&pFile, szFilePath, "wb");
		if (!pFile)
		{
			return -2;
		}

		fwrite(header, sizeof(unsigned char), 54, pFile);
		fwrite(pBuffer, sizeof(unsigned char), (size_t)(long)imageWidth * imageHeight * 4, pFile);

		fclose(pFile);
		return 0;

	}
	void DesktopProcessor::SaveToBMP(ID3D11Texture2D* src)
	{
		HRESULT hr = S_OK;
		D3D11_TEXTURE2D_DESC desc;
		src->GetDesc(&desc);

		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> stage;
		hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, stage.GetAddressOf());
		if (FAILED(hr))
		{
			return;
		}
#if 0
		D3D11_BOX box = { 0 };

		RECT rect = { 0 };
		HWND hWnd = GetForegroundWindow();
		GetWindowRect(hWnd, &rect);

		box.front = 0;
		box.back = 1;
		box.left = rect.left;
		box.right = rect.right;
		box.top = rect.top;
		box.bottom = rect.bottom;s
		m_d3dContext->CopySubresourceRegion(stage.Get(), 0, rect.left, rect.top, 0, src, 0, &box);
		static int count = 0;
		if (rect.left < 0 || rect.top < 0 || rect.right > 1920 || rect.bottom > 1080)
			printf("rect %d left:%d, top:%d, right:%d, bottom:%d \n", count++, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
#else
		m_d3dContext->CopyResource(stage.Get(), src);
#endif
		D3D11_MAPPED_SUBRESOURCE mappded = { 0 };
		hr = m_d3dContext->Map(stage.Get(), 0, D3D11_MAP_READ, 0, &mappded);
		if (FAILED(hr))
		{
			return;
		}

		static int nCount = 0;
		SaveBmp(nCount++, desc.Width, desc.Height, (unsigned char*)mappded.pData);

		m_d3dContext->Unmap(stage.Get(), 0);
	}

}