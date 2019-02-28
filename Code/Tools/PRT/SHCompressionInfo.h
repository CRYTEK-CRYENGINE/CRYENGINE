#pragma once
#include "PRTTypes.h"

namespace NSH
{
	//!< describes the compression for each material(set of decompression values and so on)
	struct SCompressionInfo
	{
		TFloatPairVec compressionValue;						//!< compression scale values and center offset for direct coeffs, individual for each mesh and material
		bool isCompressed;												//!< indicates the compression
		SCompressionInfo() : isCompressed(false), compressionValue((const uint32)0) {}
	};

	typedef prtvector<NSH::SCompressionInfo> TCompressionInfo;
}