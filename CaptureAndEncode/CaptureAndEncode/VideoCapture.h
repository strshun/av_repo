#ifndef _DRAGON_VIDEOCAPTURE_82DF7D19_958A_40A0_AD1E_719751CDBB99_H_
#define _DRAGON_VIDEOCAPTURE_82DF7D19_958A_40A0_AD1E_719751CDBB99_H_

#include <memory>
#include <mutex>
#include "DesktopDuplication.h"
#include "DesktopProcessor.h"
#include "VideoEncoder.h"
#include "FpsController.h"
#include "DragonVideo.h"

namespace Dragon {
	class VideoCapture {

	public:
		struct VideoConfig {
			int encodeWidth;		//编码宽
			int encodeHeight;		//编码高
			int videoForamt;		//编码格式
			int bitrate;			//比特率
			int bitateMode;			//比特率控制模式
			int maxFps;				//最大帧率
			int minFps;				//最小帧率
			int fpsMode;			//帧率控制模式
			int clientType;			//客户端类型
			int outputDelay;		//nvenc outputDelay
#ifdef DRAGON_MULTIPLE_ENCODE
			int clientId;
#endif
		};

		static std::unique_ptr<VideoCapture> Create(VideoConfig config);
		VideoCapture(VideoConfig config);
		~VideoCapture();

		int AcquireFrame(PDXGIFRAME frame, bool withCursor, int* cursorType);

		bool ReconfigVideoCapture(VideoConfig config);
		void DestroyVideoCapture();
#ifdef DRAGON_MULTIPLE_ENCODE
	public:
		bool Init();
#else
	protected:
		bool Init();
#endif

	private:
		VideoConfig m_config;

		ID3D11Device* m_d3dDevice = nullptr;
		ID3D11DeviceContext* m_d3d11DeviceContext = nullptr;
		IDXGIOutput* m_dxgiOutput = nullptr;

		std::unique_ptr<DesktopDuplication> m_duplication;
		std::unique_ptr<DesktopProcessor> m_renderer;
		std::unique_ptr<FpsController> m_fpsController;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_renderStage;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_lastStage;

		VideoEncoder* m_videoEncoder = nullptr;
		FrameData m_capedFrame; //捕获的桌面纹理数据
		EncodedFrame m_encdFrame; //编码后的视频帧
		std::mutex m_d3dMutex;

	};
}
#endif
