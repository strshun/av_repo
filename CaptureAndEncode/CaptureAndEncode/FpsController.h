#ifndef _DRAGON_FPSCONTROLLER_BDC97916_F7EA_410C_8BE5_8C41B2DAA428_H_
#define _DRAGON_FPSCONTROLLER_BDC97916_F7EA_410C_8BE5_8C41B2DAA428_H_

#include <memory>

#define FPSMODE_SCREEN			0x1		//��Ļ֡��
#define FPSMODE_FIX				0x2		//�̶�֡��
#define FPSMODE_FLOATING		0x3		//����֡�� min-max

namespace Dragon {

	//��һ��Ϊͳ������
	//֡������ԣ�һ����60HzΪĬ�����֡������ʵ����Ļ֡������60Hz����֤����minFps������£�֡���ƽ��


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

		//����get frame�źţ��ж��Ƿ����֡
		bool AcquireSignal(long capResult);

	private:
		
		FpsControllerConfig m_config;


	};
}


#endif
