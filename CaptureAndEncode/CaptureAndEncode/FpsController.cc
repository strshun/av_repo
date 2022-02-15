#include "FpsController.h"
#include <time.h>
#include <Windows.h>

namespace Dragon {
	
	FpsController::FpsController(FpsControllerConfig config)
		: m_config(config)
	{
	}

	std::unique_ptr<FpsController> FpsController::Create(FpsControllerConfig config)
	{
		std::unique_ptr<FpsController> fps(new FpsController(config));
		return fps;
	}

	bool FpsController::AcquireSignal(long capResult)
	{
		return true;
		static clock_t lastFrame = 0;
		int maxGap = 0;
		switch (m_config.mode)
		{
		case FPSMODE_SCREEN:
			if(capResult == S_OK)
			{
				return true;
			}
			return false;

		case FPSMODE_FIX:   //策略，原生帧优先保留，算法待定
			maxGap = 1000 / m_config.fixFps;
			if (capResult == S_OK)
			{
				//if (clock() - lastFrame < maxGap) // 丢帧
				//{
				//	return false;
				//}
				lastFrame = clock();
				return true;
			}
			if (clock() - lastFrame > maxGap)// 补帧
			{
				lastFrame = clock();
				return true;
			}
			return false;
		case FPSMODE_FLOATING:
			maxGap = 1000 / m_config.maxFps;
			if (capResult == S_OK)
			{
				lastFrame = clock();
				return true;
			}
			if (clock() - lastFrame > maxGap)
			{
				lastFrame = clock();
				return true;
			}
			return false;
		}
		return false;
	}

}