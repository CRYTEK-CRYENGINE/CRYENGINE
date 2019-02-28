// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"

#include "ControllerPQ.h"


namespace ControllerHelper
{

	ITrackPositionStorage* ConstructPositionController(ECompressionFormat format)
	{
		switch(format)
		{
		case eNoCompress:
		case eNoCompressVec3:
			return new NoCompressPosition;

		default:
			return nullptr;
		}
	}

	ITrackPositionStorage* ConstructScaleController(ECompressionFormat format)
	{
		switch (format)
		{
		case eNoCompress:
		case eNoCompressVec3:
			return new NoCompressPosition;

		default:
			return nullptr;
		}
	}

	ITrackRotationStorage* ConstructRotationController(ECompressionFormat format)
	{
		switch(format)
		{
		case eNoCompress:
		case eNoCompressQuat:
			return new NoCompressRotation;

		case eSmallTree48BitQuat:
			return new SmallTree48BitQuatRotation;

		case eSmallTree64BitQuat:
			return new SmallTree64BitQuatRotation;

		case eSmallTree64BitExtQuat:
			return new SmallTree64BitExtQuatRotation;

		default:
			return nullptr;
		}
	}

	IKeyTimesInformation* ConstructKeyTimesController(uint32 format)
	{
		switch(format)
		{
		case eF32:
			return new F32KeyTimesInformation;

		case eUINT16:
			return new UINT16KeyTimesInformation;

		case eByte:
			return new ByteKeyTimesInformation;

		case eBitset:
			return new CKeyTimesInformationBitSet;

		default:
			return nullptr;
		}
	}

} // namespace ControllerHelper


uint8 CKeyTimesInformationBitSet::m_bTable[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,
4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,
3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,
4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,
4,5,5,6,4,5,5,6,5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,
6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5,6,6,7,6,7,7,8};
