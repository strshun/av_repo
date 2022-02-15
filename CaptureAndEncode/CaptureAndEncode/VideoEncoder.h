#ifndef _DRAGON_VIDEOENCODER_4C7BEA33_DFAA_4A71_BE41_93BB335FD6E2_H_
#define _DRAGON_VIDEOENCODER_4C7BEA33_DFAA_4A71_BE41_93BB335FD6E2_H_


#include <d3d11.h>
#include <dxgi.h>

#define MAX_VIDEOFRAME_SIZE			(32 * 1024 * 1024)

namespace Dragon {

	class EncodedFrame {
	public:
		//__IN
		void* srcTexture;
		//__OUT
		unsigned char* m_pBuffer;
		int m_length;
		int m_index;
		int m_frameType;
		int m_width;
		int m_height;

		EncodedFrame()
		{
			m_pBuffer = new unsigned char[MAX_VIDEOFRAME_SIZE]();
		}
		void CopyData(int length, int index, int frameType, int width, int height, unsigned char* buffer)
		{
			if (buffer && length > 0)
			{
				memcpy(m_pBuffer, buffer, length);
			}
			m_length = length;
			m_index = index;
			m_frameType = frameType;
			m_width = width;
			m_height = height;
		}
		void ClearBuffer()
		{
			m_length = 0;
			m_index = 0;
			m_width = 0;
			m_height = 0;
			m_frameType = 0;
			//memset(m_pBuffer, 0x0, MAX_VIDEOFRAME_SIZE);
		}
		~EncodedFrame() 
		{
			if (m_pBuffer)
			{
				delete[] m_pBuffer;
				m_pBuffer = nullptr;
			}
		}
	};
	
	typedef enum tagENCODERTYPE_ {
		X264_ENCODER,
		NVENC_ENCODER,
	}ENCODERTYPE;

	class VideoEncoder {
	public:
		struct VideoEncoderConfig {
			int encodeWidth;
			int encodeHeight;
			bool hevc;
			int extraOutputDelay;
			int clientType;
			int bitrate;
			int bitrateMode;
			int fps;
		};
		static VideoEncoder* Create(ENCODERTYPE type);
		virtual bool Start(VideoEncoderConfig config) = 0;
		virtual bool Reconfig(VideoEncoderConfig config) = 0;
		virtual void Stop() = 0;
		virtual void GetBitrate(bool& autoBitarte, int& bitrate) = 0;
		virtual void SetAutoBitrate(int bitrate) = 0;
		virtual int DoEncode(EncodedFrame& frame, bool keyFrame) = 0;
		virtual void EndEncode() = 0;
		virtual void ResetFrameCount() = 0;
		virtual void SetD3DContext(void* d3dDevice, void* d3dContext) = 0;

	};

}



#endif
