#ifndef _DRAGON_NVENCODERZEROLATENCY_24602AA6_383E_4531_8D91_16FC83A1DE57_H_
#define _DRAGON_NVENCODERZEROLATENCY_24602AA6_383E_4531_8D91_16FC83A1DE57_H_

#include <stdio.h>
#include "VideoEncoder.h"
#include "NvEncoderCuda.h"
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cuda_d3d11_interop.h>


namespace Dragon {

	class NvEncZeroLatency :
		public VideoEncoder
	{
	public:
		static NvEncZeroLatency* Create();
		NvEncZeroLatency();
		~NvEncZeroLatency();

		virtual bool Start(VideoEncoderConfig config) override;
		virtual void Stop() override;
		virtual bool Reconfig(VideoEncoderConfig config) override;


		virtual void GetBitrate(bool& autoBitarte, int& bitrate) override;
		virtual void SetAutoBitrate(int bitrate) override;

		virtual void ResetFrameCount() override;
		virtual void SetD3DContext(void* d3dDevice, void* d3dContext) override;

		virtual int DoEncode(EncodedFrame& frame, bool keyFrame)override;
		virtual void EndEncode() override;

	protected:
		bool Init();
		bool ChangeBitrate(int bitrate);

	private:
		CUcontext m_cudaCtx = nullptr;

		NvEncoderCuda* m_cudaEncoder = nullptr;
		ID3D11Device* m_d3d11Device = nullptr;
		ID3D11DeviceContext* m_d3d11Context = nullptr;

		VideoEncoderConfig m_config;
		ID3D11Texture2D* m_pTexture;
		ID3D11Texture2D* m_tmp;
		cudaArray_t m_mappedArray;
		cudaGraphicsResource_t m_mappedResource = nullptr;
		
		FILE* m_file = nullptr;

		NV_ENC_INITIALIZE_PARAMS initializeParams;
		NV_ENC_CONFIG encodeConfig;
	};

}


#endif
