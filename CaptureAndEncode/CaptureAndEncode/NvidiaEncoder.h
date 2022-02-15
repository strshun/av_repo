#ifndef _DRAGON_NVENCODER_E4B9ADFE_AA25_422A_BA51_4433E658D33A_H_
#define _DRAGON_NVENCODER_E4B9ADFE_AA25_422A_BA51_4433E658D33A_H_

#include <d3d11.h>
#include <dxgi.h>

#include <nvEncodeAPI.h>
#include "VideoEncoder.h"

typedef NVENCSTATUS NVENCAPI NVENCODEAPICREATEINSTANCE(NV_ENCODE_API_FUNCTION_LIST* functionList);

namespace Dragon {

	typedef struct tagNVENCINPUTFRAME_ {
		void* inputPtr = nullptr;
		uint32_t chromaOffsets[2];
		uint32_t numChromaPlanes;
		uint32_t pitch;
		uint32_t chromaPitch;
		NV_ENC_BUFFER_FORMAT bufferFormat;
		NV_ENC_INPUT_RESOURCE_TYPE resourceType;
	}NVENCINPUTFRAME, *LPNVENCINPUTPFRAME;

	class NvidiaEncoder :
		public VideoEncoder
	{

	public:
		static NvidiaEncoder* Create();
		NvidiaEncoder();
		~NvidiaEncoder();

		virtual bool Start(VideoEncoderConfig config) override;
		virtual void Stop() override;
		virtual bool Reconfig(VideoEncoderConfig config) override;

		
		virtual void GetBitrate(bool& autoBitarte, int& bitrate) override;
		virtual void SetAutoBitrate(int bitrate) override;

		virtual void ResetFrameCount() override;
		virtual void SetD3DContext(void* d3dDevice, void* d3dContext) override;

		//nvenc
		bool CreateVideoEncoder();
		void DestroyVideoEncoder();


		virtual int DoEncode(EncodedFrame& frame, bool keyFrame)override;
		bool GetEncodedPacket(EncodedFrame& frame);
		virtual void EndEncode() override;
		
		const NV_ENC_BUFFER_FORMAT m_bufferFormat = NV_ENC_BUFFER_FORMAT_ABGR;

	protected:
		bool ChangeBitrate(int bitrate);
		bool LoadnvEncApis();

		bool InitNvEncoder();
		bool InitializeBitstreamBuffer();
		void ReleaseBitstreamBuffer();
		bool InitializeInputBuffer();
		void ReleaseInputBuffer();

	private:
		HMODULE m_nvEncModule;
		NV_ENCODE_API_FUNCTION_LIST m_nvEncApi;

		ID3D11Device* m_d3dDevice = nullptr;
		ID3D11DeviceContext* m_d3dContext = nullptr;

		int m_totalFrames = 0;
		HANDLE m_hEncoderLock;

		void* m_nvEncoderHandle = nullptr;
		NV_ENC_CONFIG m_encodeConfig;
		NV_ENC_INITIALIZE_PARAMS m_initParams;
		int32_t m_encodeBuffer = 0;
		int32_t m_nOutputDelay = 0;
		HANDLE* m_hEncoderEvents;

		NV_ENC_CREATE_BITSTREAM_BUFFER* m_outputBitstream;
		LPNVENCINPUTPFRAME m_inputBuffer;
		NV_ENC_REGISTERED_PTR* m_vRegisteredResources;
		NV_ENC_INPUT_PTR* m_vMappedInputBuffers;

		int m_iToSend = 0; //发送编码的帧数
		int m_iGot = 0;	//接收到的帧数量

		VideoEncoderConfig m_config;

	};


}


#endif

