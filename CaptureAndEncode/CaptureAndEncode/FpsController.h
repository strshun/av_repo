#ifndef _DRAGON_FPSCONTROLLER_BDC97916_F7EA_410C_8BE5_8C41B2DAA428_H_
#define _DRAGON_FPSCONTROLLER_BDC97916_F7EA_410C_8BE5_8C41B2DAA428_H_

#include <memory>

#define FPSMODE_SCREEN			0x1		//屏幕帧率
#define FPSMODE_FIX				0x2		//固定帧率
#define FPSMODE_FLOATING		0x3		//浮动帧率 min-max

namespace Dragon {

	//以一秒为统计周期
	//帧输出策略，一般以60Hz为默认输出帧数，若实际屏幕帧数不足60Hz，保证满足minFps的情况下，帧间隔平均


	class FpsController {

	public:
		struct FpsControllerConfig {
			int mode = FPSMODE_FIX;
			int minFps = 30;
			int fixFps = 59;
			int maxFps = 100;
		};

		FpsController(FpsControllerConfig config);
		~FpsController() = default;
		static std::unique_ptr<FpsController> Create(FpsControllerConfig config);

		//处理get frame信号，判断是否输出帧
		bool AcquireSignal(long capResult);

	private:
		
		FpsControllerConfig m_config;


	};
}


#endif
