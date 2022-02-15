#include "VideoEncoder.h"
#include "NvidiaEncoder.h"
#include "NvEncZeroLatency.h"
namespace Dragon {
	VideoEncoder * VideoEncoder::Create(ENCODERTYPE type)
	{
		if (type == X264_ENCODER)
		{
			return new NvidiaEncoder;
		}
		else if (type == NVENC_ENCODER)
		{
			return NvEncZeroLatency::Create();
		}
		return nullptr;
	}
}