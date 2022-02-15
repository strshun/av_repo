#include "NvidiaEncoder.h"
#include <time.h>
#include <vector>
#include <WinSock2.h>
#pragma warning(disable : 4996)
namespace Dragon {

	NvidiaEncoder* NvidiaEncoder::Create()
	{
		NvidiaEncoder* extEncoder = new NvidiaEncoder();
		return extEncoder;
	}

	bool NvidiaEncoder::Start(VideoEncoderConfig config)
	{
		m_config = config;
		if (!CreateVideoEncoder())
		{
			return false;
		}
		printf("init encoderExt success. ");
		return true;
	}

	void NvidiaEncoder::Stop()
	{
		DestroyVideoEncoder();
	}

	bool NvidiaEncoder::Reconfig(VideoEncoderConfig config)
	{
		if (config.encodeWidth != m_config.encodeWidth
			|| config.encodeHeight != m_config.encodeHeight
			|| config.hevc != m_config.hevc
			|| config.extraOutputDelay != m_config.extraOutputDelay)
		{
			DestroyVideoEncoder();
			memcpy(&m_config, &config, sizeof(VideoEncoderConfig));
			bool err = CreateVideoEncoder();
			if (err)
			{
				printf("video encoder reconfig success \n");
			}
			return err;
		}
		else if (config.bitrate != m_config.bitrate)
		{
			printf("=======================================try to change bitrate to %d======================== \n", config.bitrate);
			m_config.bitrate = config.bitrate;
			return ChangeBitrate(config.bitrate);
		}
		printf("==============================video encoder does not need to reconfig===\n");
		return true;
	}

	bool NvidiaEncoder::ChangeBitrate(int bitrate)
	{
		DWORD dwRet = WaitForSingleObject(m_hEncoderLock, 3000);
		if (dwRet != WAIT_OBJECT_0)
		{
			printf("reconfig ext encoder timeout. ");
			return false;
		}
		NV_ENC_RECONFIGURE_PARAMS reconfigParam = { NV_ENC_RECONFIGURE_PARAMS_VER };
		reconfigParam.forceIDR = 1;
		reconfigParam.resetEncoder = 1;
		memcpy(&(reconfigParam.reInitEncodeParams), &m_initParams, sizeof(m_initParams));
		if (m_initParams.presetGUID != NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID &&
			m_initParams.presetGUID != NV_ENC_PRESET_LOSSLESS_HP_GUID)
		{
			//m_encodeConfig.rcParams.maxBitRate = bitrate * 1200;
			//m_encodeConfig.rcParams.averageBitRate = bitrate * 1000;
            reconfigParam.reInitEncodeParams.encodeConfig->rcParams.maxBitRate = bitrate * 1200;
            reconfigParam.reInitEncodeParams.encodeConfig->rcParams.averageBitRate = bitrate * 1000;

			m_encodeConfig.rcParams.maxBitRate = bitrate * 1000;
            m_encodeConfig.rcParams.averageBitRate = bitrate * 1000;
			
		}
		NVENCSTATUS nvStatus = m_nvEncApi.nvEncReconfigureEncoder(m_nvEncoderHandle, &reconfigParam);
		if (nvStatus != NV_ENC_SUCCESS)
		{
			printf("reconfig bitrate failed:%d. ", nvStatus);
			ReleaseMutex(m_hEncoderLock);
			return false;
		}
		memcpy(&m_initParams, &(reconfigParam.reInitEncodeParams), sizeof(m_initParams));
		if (reconfigParam.reInitEncodeParams.encodeConfig)
		{
			memcpy(&m_encodeConfig, reconfigParam.reInitEncodeParams.encodeConfig, sizeof(m_encodeConfig));
		}
		m_config.bitrate = bitrate;
		ReleaseMutex(m_hEncoderLock);
		printf("reconfig bitrate success:%d. ", m_config.bitrate);
		return true;
	}

	void NvidiaEncoder::GetBitrate(bool & autoBitarte, int & bitrate)
	{
		autoBitarte = m_config.bitrateMode;
		bitrate = m_config.bitrate;
	}

	void NvidiaEncoder::SetAutoBitrate(int bitrate)
	{
		
	}

	void NvidiaEncoder::ResetFrameCount()
	{
		m_totalFrames = 0; 
		printf("reset encoded frame count to zero. ");
	}

	void NvidiaEncoder::SetD3DContext(void * d3dDevice, void * d3dContext)
	{
		m_d3dDevice = (ID3D11Device*)d3dDevice;
		m_d3dContext = (ID3D11DeviceContext*)d3dContext;
	}
	
	NvidiaEncoder::NvidiaEncoder()
	{
		LoadnvEncApis();
		m_hEncoderLock = CreateMutex(NULL, FALSE, NULL);
	}

	NvidiaEncoder::~NvidiaEncoder()
	{
		Stop();

		WaitForSingleObject(m_hEncoderLock, INFINITE);
		CloseHandle(m_hEncoderLock);
		m_hEncoderLock = INVALID_HANDLE_VALUE;

		if (m_nvEncModule)
		{
			FreeLibrary(m_nvEncModule);
		}
	}

	bool NvidiaEncoder::LoadnvEncApis()
	{
		m_nvEncModule = LoadLibrary(L"nvEncodeAPI64.dll");
		if (!m_nvEncModule)
		{
			MessageBox(NULL, L"找不到 nvEncodeAPI64.dll.", L"警告", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
			exit(0);
			return false;
		}

		NVENCODEAPICREATEINSTANCE* nvEncInitInstance = (NVENCODEAPICREATEINSTANCE *)GetProcAddress(m_nvEncModule, "NvEncodeAPICreateInstance");
		if (!nvEncInitInstance)
		{
			FreeLibrary(m_nvEncModule);
			m_nvEncModule = NULL;
			return false;
		}

		m_nvEncApi.version = NV_ENCODE_API_FUNCTION_LIST_VER;
		NVENCSTATUS status = nvEncInitInstance(&m_nvEncApi);
		if (status != NV_ENC_SUCCESS)
		{
			FreeLibrary(m_nvEncModule);
			m_nvEncModule = NULL;
			return false;
		}

		return true;
	}

	bool NvidiaEncoder::CreateVideoEncoder()
	{
		if (!InitNvEncoder())
		{
			printf("can not init nv encoder. ");
			return false;
		}
		if (!InitializeBitstreamBuffer())
		{
			printf("can not init bitstream buffer. ");
			return false;
		}
		if (!InitializeInputBuffer())
		{
			printf("can not init input buffer. ");
			return false;
		}
		return true;
	}

	void NvidiaEncoder::DestroyVideoEncoder()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		EndEncode();

		ReleaseBitstreamBuffer();
		ReleaseInputBuffer();

		//unregist events
		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			NV_ENC_EVENT_PARAMS eventParams = { 0 };
			eventParams.version = NV_ENC_EVENT_PARAMS_VER;
			eventParams.completionEvent = m_hEncoderEvents[i];
			nvStatus = m_nvEncApi.nvEncUnregisterAsyncEvent(m_nvEncoderHandle, &eventParams);
		}
		delete[]m_hEncoderEvents;
		m_hEncoderEvents = nullptr;
		nvStatus = m_nvEncApi.nvEncDestroyEncoder(m_nvEncoderHandle);
		m_nvEncoderHandle = nullptr;

	}

	bool NvidiaEncoder::InitNvEncoder()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;

		GUID codecGUID = NV_ENC_CODEC_H264_GUID;
		//GUID presetGUID = NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
		GUID presetGUID = NV_ENC_PRESET_DEFAULT_GUID;
		GUID profileGUID = NV_ENC_H264_PROFILE_BASELINE_GUID;

		if (m_config.hevc)
		{
			codecGUID = NV_ENC_CODEC_HEVC_GUID;
			profileGUID = NV_ENC_HEVC_PROFILE_MAIN_GUID;
		}
		
		//opensession
		NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS param = { 0 };
		param.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
		param.apiVersion = NVENCAPI_VERSION;
		param.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
		param.device = (void*)m_d3dDevice;
		nvStatus = m_nvEncApi.nvEncOpenEncodeSessionEx(&param, &m_nvEncoderHandle);
		if (nvStatus != NV_ENC_SUCCESS) { return false; }

		//获取预设编码器配置
		NV_ENC_PRESET_CONFIG presetConfig = { 0 };
		presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
		presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
		nvStatus = m_nvEncApi.nvEncGetEncodePresetConfig(m_nvEncoderHandle, codecGUID,
			presetGUID, &presetConfig);
		if (nvStatus != NV_ENC_SUCCESS) { return false; }

		memset(&m_encodeConfig, 0x0, sizeof(NV_ENC_CONFIG));
		memset(&m_initParams, 0x0, sizeof(NV_ENC_INITIALIZE_PARAMS));
		m_initParams.encodeConfig = &m_encodeConfig;
		m_initParams.encodeConfig->version = NV_ENC_CONFIG_VER;
		m_initParams.version = NV_ENC_INITIALIZE_PARAMS_VER;

		m_initParams.encodeGUID = codecGUID;
		m_initParams.presetGUID = presetGUID;
		m_initParams.encodeWidth = m_config.encodeWidth;
		m_initParams.encodeHeight = m_config.encodeHeight;
		m_initParams.darWidth = m_initParams.encodeWidth;
		m_initParams.darHeight = m_initParams.encodeHeight;
		m_initParams.frameRateNum = m_config.fps;
		m_initParams.frameRateDen = 1;
		m_initParams.enableEncodeAsync = 1;
		m_initParams.enablePTD = 1;
		m_initParams.reportSliceOffsets = 0; // if set 1 need enableEncodeAsync == 0
		m_initParams.enableSubFrameWrite = 0;
		m_initParams.enableExternalMEHints = 0;
		m_initParams.enableMEOnlyMode = 0;
		m_initParams.enableWeightedPrediction = 0;

		m_initParams.maxEncodeWidth = 0;
		m_initParams.maxEncodeHeight = 0;

		memcpy(&m_encodeConfig, &presetConfig.presetCfg, sizeof(NV_ENC_CONFIG));
		m_encodeConfig.profileGUID = profileGUID;
		m_encodeConfig.gopLength = NVENC_INFINITE_GOPLENGTH;
		m_encodeConfig.frameIntervalP = 1; //idr-p-p-p-p-...

		//m_encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
        m_encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;


		if (m_initParams.presetGUID != NV_ENC_PRESET_LOSSLESS_DEFAULT_GUID &&
			m_initParams.presetGUID != NV_ENC_PRESET_LOSSLESS_HP_GUID)
		{
            //动态码率
            if (NV_ENC_PARAMS_RC_VBR == m_encodeConfig.rcParams.rateControlMode) {
                m_encodeConfig.rcParams.constQP = { 28, 31, 25 };
                m_encodeConfig.rcParams.maxBitRate = m_config.bitrate * 1200;
                m_encodeConfig.rcParams.averageBitRate = m_config.bitrate * 1000;
            }
            //常码率（弃用）
            else if (NV_ENC_PARAMS_RC_CBR == m_encodeConfig.rcParams.rateControlMode) {
                m_encodeConfig.rcParams.maxBitRate = 12000 * 1000;
                m_encodeConfig.rcParams.averageBitRate = 12000 * 1000;
            }
            //QP（弃用）
            else if (NV_ENC_PARAMS_RC_CONSTQP == m_encodeConfig.rcParams.rateControlMode) {
                m_encodeConfig.rcParams.averageBitRate = 0;
                m_encodeConfig.rcParams.vbvBufferSize = 0;
                m_encodeConfig.rcParams.vbvInitialDelay = 0;
                m_encodeConfig.rcParams.maxBitRate = 0;
                m_encodeConfig.rcParams.enableMinQP = 1;
                m_encodeConfig.rcParams.enableMaxQP = 1;
                m_encodeConfig.rcParams.minQP.qpIntra = 15;
                m_encodeConfig.rcParams.minQP.qpInterP = 15;
                m_encodeConfig.rcParams.minQP.qpInterB = 15;
                m_encodeConfig.rcParams.maxQP.qpIntra = 25;
                m_encodeConfig.rcParams.maxQP.qpInterP = 25;
                m_encodeConfig.rcParams.maxQP.qpInterB = 25;
            }
        }
		if (m_initParams.encodeGUID == NV_ENC_CODEC_H264_GUID)
		{
/*            m_encodeConfig.encodeCodecConfig.h264Config.enableIntraRefresh = 1; 
            m_encodeConfig.encodeCodecConfig.h264Config.enableConstrainedEncoding = 1;*/
            m_encodeConfig.encodeCodecConfig.h264Config.idrPeriod = m_encodeConfig.gopLength;
		}
		if (m_initParams.encodeGUID == NV_ENC_CODEC_HEVC_GUID)
		{
			m_encodeConfig.encodeCodecConfig.hevcConfig.idrPeriod = m_encodeConfig.gopLength;
		}
#if 0
		m_encodeConfig.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR_LOWDELAY_HQ;
		m_encodeConfig.rcParams.vbvBufferSize = (m_encodeConfig.rcParams.averageBitRate * m_initParams.frameRateDen / m_initParams.frameRateNum) * 5;
		m_encodeConfig.rcParams.maxBitRate = m_encodeConfig.rcParams.averageBitRate;
		m_encodeConfig.rcParams.vbvInitialDelay = m_encodeConfig.rcParams.vbvBufferSize;
#endif
		nvStatus = m_nvEncApi.nvEncInitializeEncoder(m_nvEncoderHandle, &m_initParams);
		if (nvStatus != NV_ENC_SUCCESS) { return false; }

		m_encodeBuffer = m_encodeConfig.frameIntervalP + m_encodeConfig.rcParams.lookaheadDepth + m_config.extraOutputDelay;
        m_nOutputDelay = m_encodeBuffer - 1;

		printf("encode buffer:%d, outputdelay:%d \n", m_encodeBuffer, m_nOutputDelay);

		m_hEncoderEvents = new HANDLE[m_encodeBuffer];
		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			m_hEncoderEvents[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
			NV_ENC_EVENT_PARAMS eventParams = { 0 };
			eventParams.version = NV_ENC_EVENT_PARAMS_VER;
			eventParams.completionEvent = m_hEncoderEvents[i];
			nvStatus = m_nvEncApi.nvEncRegisterAsyncEvent(m_nvEncoderHandle, &eventParams);
			if (nvStatus != NV_ENC_SUCCESS) { return false; }
		}
		return true;
	}

	bool NvidiaEncoder::InitializeBitstreamBuffer()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		m_outputBitstream = new (std::nothrow)NV_ENC_CREATE_BITSTREAM_BUFFER[m_encodeBuffer];
		if (!m_outputBitstream)
		{
			return false;
		}
		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			m_outputBitstream[i].version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
			nvStatus = m_nvEncApi.nvEncCreateBitstreamBuffer(m_nvEncoderHandle, &m_outputBitstream[i]);
			if (nvStatus != NV_ENC_SUCCESS) { return false; }
		}
		return true;
	}

	bool NvidiaEncoder::InitializeInputBuffer()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		m_inputBuffer = new NVENCINPUTFRAME[m_encodeBuffer];
		m_vRegisteredResources = new NV_ENC_REGISTERED_PTR[m_encodeBuffer];
		m_vMappedInputBuffers = new NV_ENC_INPUT_PTR[m_encodeBuffer];

		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			ID3D11Texture2D *pInputTextures = nullptr;
			D3D11_TEXTURE2D_DESC desc = { 0 };
			desc.Width = m_config.encodeWidth;
			desc.Height = m_config.encodeHeight;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET;
			desc.CPUAccessFlags = 0;
			HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &pInputTextures);
			if (FAILED(hr)) { return false; }

			NV_ENC_REGISTER_RESOURCE registerResource = { NV_ENC_REGISTER_RESOURCE_VER };
			registerResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
			registerResource.resourceToRegister = (void *)pInputTextures;
			registerResource.width = m_config.encodeWidth;
			registerResource.height = m_config.encodeHeight;
			registerResource.pitch = 0;
			registerResource.bufferFormat = m_bufferFormat;

			nvStatus = m_nvEncApi.nvEncRegisterResource(m_nvEncoderHandle, &registerResource);
			if (nvStatus != NV_ENC_SUCCESS) { return false; }

			m_inputBuffer[i].inputPtr = pInputTextures;
			m_inputBuffer[i].chromaOffsets[0] = 0;
			m_inputBuffer[i].chromaOffsets[1] = 0;
			m_inputBuffer[i].numChromaPlanes = 0;
			m_inputBuffer[i].pitch = 0;
			m_inputBuffer[i].chromaPitch = 0;
			m_inputBuffer[i].bufferFormat = m_bufferFormat;
			m_inputBuffer[i].resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;

			m_vRegisteredResources[i] = registerResource.registeredResource;
		}

		return true;
	}

	/*
	0 success to encode and get a frame
	1 desktop do not change and can not capture;
	2 nv encoder need more input;
	3 nv encoder is now reconfiguring;
	-1 map resource failed;
	-2 get encoded frame failed;
	-3 do encode failed ;
	*/
	int NvidiaEncoder::DoEncode(EncodedFrame & frame, bool keyFrame)
	{
		DWORD dwRet = WaitForSingleObject(m_hEncoderLock, 1000);
		if (dwRet != WAIT_OBJECT_0)
		{
			return 1;
		}
		clock_t clStart = clock();
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		int inputIndex = m_iToSend % m_encodeBuffer;
		ID3D11Texture2D* pTexture = (ID3D11Texture2D*)frame.srcTexture;
		ID3D11Texture2D *pTexBgra = (ID3D11Texture2D*)m_inputBuffer[inputIndex].inputPtr;

		m_d3dContext->CopyResource(pTexBgra, pTexture);

#if 0
		D3D11_TEXTURE2D_DESC desc;
		ID3D11Texture2D* stage = nullptr;
		((ID3D11Texture2D*)frame.srcTexture)->GetDesc(&desc);
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.BindFlags = 0;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		desc.MiscFlags = 0;
		HRESULT hr = m_d3dDevice->CreateTexture2D(&desc, nullptr, &stage);
		m_d3dContext->CopyResource(stage, (ID3D11Texture2D*)frame.srcTexture);

		D3D11_MAPPED_SUBRESOURCE mapRes;
		hr = m_d3dContext->Map(stage, 0, D3D11_MAP_READ, 0, &mapRes);
		m_d3dContext->Unmap(stage, 0);
		stage->Release();
#endif

		m_totalFrames += 1;

		NV_ENC_MAP_INPUT_RESOURCE mapInputResource = { NV_ENC_MAP_INPUT_RESOURCE_VER };
		mapInputResource.registeredResource = m_vRegisteredResources[inputIndex];
		nvStatus = m_nvEncApi.nvEncMapInputResource(m_nvEncoderHandle, &mapInputResource);
		if (nvStatus != NV_ENC_SUCCESS) 
		{ 
			printf("nv encode map input resource failed. ");
			ReleaseMutex(m_hEncoderLock);
			return -1; 
		}
		m_vMappedInputBuffers[inputIndex] = mapInputResource.mappedResource;
		NV_ENC_PIC_PARAMS encParam = { 0 };
		encParam.version = NV_ENC_PIC_PARAMS_VER;
		encParam.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
		encParam.inputBuffer = m_vMappedInputBuffers[inputIndex];
		encParam.bufferFmt = m_bufferFormat;
		encParam.outputBitstream = m_outputBitstream[inputIndex].bitstreamBuffer;
		encParam.completionEvent = m_hEncoderEvents[inputIndex];
		encParam.inputWidth = m_config.encodeWidth;
        encParam.inputHeight = m_config.encodeHeight;
		encParam.inputTimeStamp = clock();
		if (keyFrame)
		{
            //static uint64_t lastTimestamp = encParam.inputTimeStamp;
            //if (lastTimestamp != 0) {
            //    m_nvEncApi.nvEncInvalidateRefFrames(m_nvEncoderHandle, encParam.inputTimeStamp);
            //}
            //lastTimestamp = encParam.inputTimeStamp;
            //encParam.codecPicParams.h264PicParams.refPicFlag = 1;
            //encParam.codecPicParams.h264PicParams.ltrMarkFrame = 1;
            //encParam.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA;
            //encParam.codecPicParams.h264PicParams.constrainedFrame = 1;
            //encParam.codecPicParams.h264PicParams.displayPOCSyntax = 1;
            //encParam.codecPicParams.h264PicParams.refPicFlag = 1;
            //encParam.pictureType = NV_ENC_PIC_TYPE_I;
            encParam.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;
        }
		else
		{
			encParam.encodePicFlags = 0;
		}
		nvStatus = m_nvEncApi.nvEncEncodePicture(m_nvEncoderHandle, &encParam);
		if (nvStatus == NV_ENC_SUCCESS || nvStatus == NV_ENC_ERR_NEED_MORE_INPUT)
		{
			m_iToSend++;
			if (!GetEncodedPacket(frame))
			{
				ReleaseMutex(m_hEncoderLock);
				return -2;
			}
		}
		else
		{
			ReleaseMutex(m_hEncoderLock);
			printf("nv encoder failed. ");
			return -3;
		}
		ReleaseMutex(m_hEncoderLock);
        
#if 1
		static FILE* videoFile = nullptr;
		if (!videoFile)
		{
			fopen_s(&videoFile, "testvideo.264", "ab+");
		}
		if (videoFile)
		{
			fwrite(frame.m_pBuffer, frame.m_length, sizeof(char), videoFile);
			printf("CAP VIDEO FRAME %d \n", frame.m_length);
		}
#endif
		return 0;
	}

	bool NvidiaEncoder::GetEncodedPacket(EncodedFrame & frame)
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		int gotIndex = m_iGot % m_encodeBuffer;
		int iEnd = m_iToSend - m_nOutputDelay;
		for (; m_iGot < iEnd; m_iGot++)
		{
			DWORD dwRet = WaitForSingleObject(m_hEncoderEvents[gotIndex], 20000);
			if (dwRet != WAIT_OBJECT_0) 
			{ 
				printf("nv encoder get frame timeout 20s. ");
				return false; 
			}

			NV_ENC_LOCK_BITSTREAM lockBitstreamData = { NV_ENC_LOCK_BITSTREAM_VER };
			lockBitstreamData.outputBitstream = m_outputBitstream[gotIndex].bitstreamBuffer;
			lockBitstreamData.doNotWait = false;
			nvStatus = m_nvEncApi.nvEncLockBitstream(m_nvEncoderHandle, &lockBitstreamData);
			if (nvStatus != NV_ENC_SUCCESS) 
			{ 
				printf("nv encode lock bitstream failed. ");
				return false; 
			}
			frame.m_length = lockBitstreamData.bitstreamSizeInBytes;
			memcpy(frame.m_pBuffer, lockBitstreamData.bitstreamBufferPtr, frame.m_length);
			frame.m_frameType = lockBitstreamData.pictureType;
			frame.m_index = lockBitstreamData.frameIdx;

			nvStatus = m_nvEncApi.nvEncUnlockBitstream(m_nvEncoderHandle, lockBitstreamData.outputBitstream);
			nvStatus = m_nvEncApi.nvEncUnmapInputResource(m_nvEncoderHandle, m_vMappedInputBuffers[gotIndex]);
			m_vMappedInputBuffers[gotIndex] = nullptr;
		}

		return true;
	}

	void NvidiaEncoder::EndEncode()
	{
		NV_ENC_PIC_PARAMS picParams = { NV_ENC_PIC_PARAMS_VER };
		picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
		picParams.completionEvent = m_hEncoderEvents[m_iToSend % m_encodeBuffer];
		m_nvEncApi.nvEncEncodePicture(m_nvEncoderHandle, &picParams);

		EncodedFrame frame;
		GetEncodedPacket(frame);
		m_iGot = 0;
		m_iToSend = 0;
		m_totalFrames = 0;

	}

	void NvidiaEncoder::ReleaseInputBuffer()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		
		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			nvStatus = m_nvEncApi.nvEncUnmapInputResource(m_nvEncoderHandle, m_vMappedInputBuffers[i]);
			nvStatus = m_nvEncApi.nvEncUnregisterResource(m_nvEncoderHandle, m_vRegisteredResources[i]);
			ID3D11Texture2D *pInputTextures = (ID3D11Texture2D*)m_inputBuffer[i].inputPtr;
			pInputTextures->Release();
		}
		delete[] m_inputBuffer;
		m_inputBuffer = nullptr;
		delete[] m_vRegisteredResources;
		m_vRegisteredResources = nullptr;
		delete[] m_vMappedInputBuffers;
		m_vMappedInputBuffers = nullptr;

	}
	void NvidiaEncoder::ReleaseBitstreamBuffer()
	{
		NVENCSTATUS nvStatus = NV_ENC_SUCCESS;
		for (int32_t i = 0; i < m_encodeBuffer; i++)
		{
			nvStatus = m_nvEncApi.nvEncDestroyBitstreamBuffer(m_nvEncoderHandle, m_outputBitstream[i].bitstreamBuffer);
		}
		delete[] m_outputBitstream;
	}

	

}
