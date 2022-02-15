#include "NvEncZeroLatency.h"
#pragma comment(lib, "cuda.lib")
#pragma comment(lib, "nvencodeapi.lib")

namespace Dragon {

	static int SaveBmp(int index, int flag, int imageWidth, int imageHeight, unsigned char * pBuffer)
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
		sprintf_s(szFilePath, sizeof(szFilePath), "pic/pic%d-%d.bmp", index, flag);

		FILE *pFile = NULL;
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


	NvEncZeroLatency * Dragon::NvEncZeroLatency::Create()
	{
		NvEncZeroLatency* encoder = new NvEncZeroLatency();
		if (!encoder)
		{
			return nullptr;
		}
		if (!encoder->Init())
		{
			return nullptr;
		}
		return encoder;
	}

	Dragon::NvEncZeroLatency::NvEncZeroLatency()
	{
	}

	Dragon::NvEncZeroLatency::~NvEncZeroLatency()
	{
		Stop();
		cuCtxDestroy(m_cudaCtx);
	}

	bool NvEncZeroLatency::Start(VideoEncoderConfig config)
	{
		m_config = config;

		D3D11_TEXTURE2D_DESC desc = { 0 };
		desc.Width = config.encodeWidth;
		desc.Height = config.encodeHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

		HRESULT hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, &m_pTexture);
		if (hr != S_OK)
		{
			return false;
		}

		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.BindFlags = 0;
		desc.MiscFlags = 0;
		hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, &m_tmp);

		cudaError_t cuError = cudaGraphicsD3D11RegisterResource(&m_mappedResource, m_pTexture, cudaGraphicsRegisterFlagsNone);
		if (cuError != cudaSuccess)
		{
			return false;
		}
		
		cuError = cudaGraphicsMapResources(1, &m_mappedResource);
		if (cuError != cudaSuccess)
		{
			return false;
		}

		cuError = cudaGraphicsSubResourceGetMappedArray(&m_mappedArray, m_mappedResource, 0, 0);
		if (cuError != cudaSuccess)
		{
			return false;
		}

		m_cudaEncoder = new NvEncoderCuda(m_cudaCtx, config.encodeWidth, config.encodeHeight, NV_ENC_BUFFER_FORMAT_ARGB, 0);
		if (!m_cudaEncoder)
		{
			return false;
		}
		initializeParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
		encodeConfig.version = NV_ENC_CONFIG_VER;
		initializeParams.encodeConfig = &encodeConfig;
		if (!m_config.hevc)
		{
			m_cudaEncoder->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_H264_GUID, NV_ENC_PRESET_P3_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY);
		}
		else
		{
			m_cudaEncoder->CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_HEVC_GUID, NV_ENC_PRESET_P3_GUID, NV_ENC_TUNING_INFO_LOW_LATENCY);
		}

		encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
		encodeConfig.frameIntervalP = 1;
		if (!config.hevc)
		{
			encodeConfig.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
		}
		else
		{
			encodeConfig.encodeCodecConfig.hevcConfig.idrPeriod = NVENC_INFINITE_GOPLENGTH;
		} 

		initializeParams.frameRateNum = config.fps;
		initializeParams.frameRateDen = 1;
		encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
		encodeConfig.rcParams.multiPass = NV_ENC_TWO_PASS_FULL_RESOLUTION;
		encodeConfig.rcParams.averageBitRate = 1000 * config.bitrate;
		encodeConfig.rcParams.vbvBufferSize = (encodeConfig.rcParams.averageBitRate * initializeParams.frameRateDen / initializeParams.frameRateNum) * 5;
		encodeConfig.rcParams.maxBitRate = encodeConfig.rcParams.averageBitRate;
		encodeConfig.rcParams.vbvInitialDelay = encodeConfig.rcParams.vbvBufferSize;
		
		if (!config.hevc) 
		{
			initializeParams.encodeConfig->encodeCodecConfig.h264Config.repeatSPSPPS = 1;
		}
		else
		{
			initializeParams.encodeConfig->encodeCodecConfig.hevcConfig.repeatSPSPPS = 1;
		}

		m_cudaEncoder->CreateEncoder(&initializeParams);

		return true ;
	}

	void NvEncZeroLatency::Stop()
	{
		if (!m_cudaEncoder)
		{
			return;
		}
		EndEncode();

		m_cudaEncoder->DestroyEncoder();
		delete m_cudaEncoder;
		m_cudaEncoder = nullptr;

		cudaGraphicsUnmapResources(1, &m_mappedResource);
		cudaFree(m_mappedArray);

		if (m_tmp)
		{
			m_tmp->Release();
			m_tmp = nullptr;
		}
		if (m_pTexture)
		{
			m_pTexture->Release();
			m_pTexture = nullptr;
		}

	}

	bool NvEncZeroLatency::ChangeBitrate(int bitrate)
	{
		NV_ENC_RECONFIGURE_PARAMS reconfigureParams = { NV_ENC_RECONFIGURE_PARAMS_VER };
		memcpy(&reconfigureParams.reInitEncodeParams, &initializeParams, sizeof(initializeParams));
		NV_ENC_CONFIG reInitCodecConfig = { NV_ENC_CONFIG_VER };
		memcpy(&reInitCodecConfig, initializeParams.encodeConfig, sizeof(reInitCodecConfig));
		reconfigureParams.reInitEncodeParams.encodeConfig = &reInitCodecConfig;

		reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.averageBitRate = 1000 * bitrate;
		reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.vbvBufferSize = reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.averageBitRate *
			reconfigureParams.reInitEncodeParams.frameRateDen / reconfigureParams.reInitEncodeParams.frameRateNum;
		reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.vbvInitialDelay = reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.vbvBufferSize;

		m_cudaEncoder->Reconfigure(&reconfigureParams);
		printf("reconfig bitrate to %d \n", bitrate);
		return true;
	}

	bool NvEncZeroLatency::Reconfig(VideoEncoderConfig config)
	{
		if (config.encodeWidth != m_config.encodeWidth
			|| config.encodeHeight != m_config.encodeHeight
			|| config.hevc != m_config.hevc
			)
		{
			Stop();
			m_config = config;
			return Start(config);
		}
		else if (config.bitrate != m_config.bitrate)
		{
			m_config.bitrate = config.bitrate;
			return ChangeBitrate(config.bitrate);
		}
		return true;
	}

	void NvEncZeroLatency::GetBitrate(bool & autoBitarte, int & bitrate)
	{
	}

	void NvEncZeroLatency::SetAutoBitrate(int bitrate)
	{
	}

	void NvEncZeroLatency::ResetFrameCount()
	{
	}

	void NvEncZeroLatency::SetD3DContext(void * d3dDevice, void * d3dContext)
	{
		m_d3d11Device = (ID3D11Device*)d3dDevice;
		m_d3d11Context = (ID3D11DeviceContext*)d3dContext;
	}

	int NvEncZeroLatency::DoEncode(EncodedFrame & frame, bool keyFrame)
	{
		const NvEncInputFrame* encoderInputFrame = m_cudaEncoder->GetNextInputFrame();

		m_d3d11Context->CopyResource((ID3D11Resource*)m_pTexture, (ID3D11Resource*)frame.srcTexture);

#if 0
		m_d3d11Context->CopyResource((ID3D11Resource*)m_tmp, m_pTexture);

		static int count1 = 0;
		D3D11_MAPPED_SUBRESOURCE mapRes;
		HRESULT hr = m_d3d11Context->Map(m_tmp, 0, D3D11_MAP_READ, 0, &mapRes);
		SaveBmp(count1, 0, 1920, 1080, (unsigned char*)mapRes.pData);
		m_d3d11Context->Unmap(m_tmp, 0);
#endif

		CUDA_DRVAPI_CALL(cuCtxPushCurrent(m_cudaCtx));

		CUDA_MEMCPY2D memCpy = { 0 };
		memCpy.srcMemoryType = CU_MEMORYTYPE_ARRAY;
		memCpy.srcArray = (CUarray)m_mappedArray;

		memCpy.dstMemoryType = CU_MEMORYTYPE_DEVICE;
		memCpy.dstDevice = (CUdeviceptr)encoderInputFrame->inputPtr;
		memCpy.dstPitch = m_cudaEncoder->GetCUdeviceptrPitch();

		memCpy.WidthInBytes = m_config.encodeWidth * 4;
		memCpy.Height = m_config.encodeHeight;
		CUresult cuStatus = cuMemcpy2D(&memCpy);
		if (cuStatus != CUDA_SUCCESS)
		{
			return -1;
		}
		
		CUDA_DRVAPI_CALL(cuCtxPopCurrent(NULL));

		NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
		picParams.encodePicFlags = 0;
		if (keyFrame)
		{
			picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
		}
		std::vector<NvEncOutputFrame> frames;
		m_cudaEncoder->EncodeFrame(frames, &picParams);

		if (frames.size() > 0)
		{
			NvEncOutputFrame& tmp = frames[0];
			frame.CopyData(tmp.size, tmp.index, tmp.frameType, tmp.width, tmp.height, tmp.frameBuffer.data());
		}
#if 1
		FILE* videoFile = nullptr;
		if (!videoFile)
		{
			fopen_s(&videoFile, "testvideo.h264", "ab+");
		}
		if (videoFile)
		{
			fwrite(frame.m_pBuffer, frame.m_length, sizeof(char), videoFile);
			fclose(videoFile);
			printf("CAP VIDEO FRAME %d \n", frame.m_length);
		}
#endif

		//for (int i = 0; i < frames.size(); i++)
		//{
		//	FILE* file = nullptr;
		//	fopen_s(&file, "test.h264", "ab+");
		//	fwrite(frames[i].frameBuffer.data(), 1, frames[i].frameBuffer.size(), file);
		//	fclose(file);
		//	printf("encd one frame %d  size %d ,type %d \n", frames[i].index, frames[i].size, frames[i].frameType);
		//}
		return 0;

	}

	void NvEncZeroLatency::EndEncode()
	{
		std::vector<std::vector<uint8_t>> vPacket;
		m_cudaEncoder->EndEncode(vPacket);
	}

	bool NvEncZeroLatency::Init()
	{
		
		int iGpu = 0;
		CUresult cuStatus;
		cuStatus = cuInit(0);
		if (cuStatus != CUDA_SUCCESS)
		{
			return false;
		}
		int nGpu = 0;
		cuStatus = cuDeviceGetCount(&nGpu);
		if (cuStatus != CUDA_SUCCESS)
		{
			return false;
		}
		CUdevice cuDevice = 0;
		cuStatus = cuDeviceGet(&cuDevice, iGpu);
		if (cuStatus != CUDA_SUCCESS)
		{
			return false;
		}
		char szDeviceName[128] = { 0 };
		cuStatus = cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice);
		if (cuStatus != CUDA_SUCCESS)
		{
			return false;
		}
		printf("GPU in use %s \n", szDeviceName);
		cuStatus = cuCtxCreate(&m_cudaCtx, 0, cuDevice);
		if (cuStatus != CUDA_SUCCESS)
		{
			return false;
		}
		return true;
	}
	
}