// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ControllerPQ.h"

#include <float.h>
#include "CharacterManager.h"
#include "ControllerOpt.h"

namespace ControllerHelper
{

uint32 GetPositionsFormatSizeOf(uint32 format)
{
	switch (format)
	{
	case eNoCompress:
	case eNoCompressVec3:
		return sizeof(NoCompressVec3);

	default:
		return 0;
	}
}
uint32 GetScalingFormatSizeOf(uint32 format)
{
	return GetPositionsFormatSizeOf(format);
}

uint32 GetRotationFormatSizeOf(uint32 format)
{
	switch (format)
	{
	case eNoCompress:
	case eNoCompressQuat:
		return sizeof(NoCompressQuat);

	case eSmallTree48BitQuat:
		return sizeof(SmallTree48BitQuat);

	case eSmallTree64BitQuat:
		return sizeof(SmallTree64BitQuat);

	case eSmallTree64BitExtQuat:
		return sizeof(SmallTree64BitExtQuat);

	default:
		return 0;
	}
}

uint32 GetKeyTimesFormatSizeOf(uint32 format)
{
	switch (format)
	{
	case eF32:
		return sizeof(float);

	case eUINT16:
		return sizeof(uint16);

	case eByte:
		return sizeof(uint8);

	case eF32StartStop:
		return sizeof(float);

	case eUINT16StartStop:
		return sizeof(uint16);

	case eByteStartStop:
		return sizeof(uint8);

	case eBitset:
		return sizeof(uint16);

	default:
		return 0;
	}
}

ITrackPositionStorage* GetPositionControllerPtr(uint32 format, void* pData, size_t numElements)
{
	switch (format)
	{
	case eNoCompress:
	case eNoCompressVec3:
		return new NoCompressPosition(reinterpret_cast<NoCompressVec3*>(pData), numElements);

	default:
		return nullptr;
	}
}

ITrackPositionStorage* GetScalingControllerPtr(uint32 format, void* pData, size_t numElements)
{
	return GetPositionControllerPtr(format, pData, numElements);
}

ITrackRotationStorage* GetRotationControllerPtr(uint32 format, void* pData, size_t numElements)
{
	switch (format)
	{
	case eNoCompress:
	case eNoCompressQuat:
		return new NoCompressRotation(reinterpret_cast<NoCompressQuat*>(pData), numElements);

	case eSmallTree48BitQuat:
		return new SmallTree48BitQuatRotation(reinterpret_cast<SmallTree48BitQuat*>(pData), numElements);

	case eSmallTree64BitQuat:
		return new SmallTree64BitQuatRotation(reinterpret_cast<SmallTree64BitQuat*>(pData), numElements);

	case eSmallTree64BitExtQuat:
		return new SmallTree64BitExtQuatRotation(reinterpret_cast<SmallTree64BitExtQuat*>(pData), numElements);

	default:
		return nullptr;
	}
}

IKeyTimesInformation* GetKeyTimesControllerPtr(uint32 format, void* pData, size_t numElements)
{
	switch (format)
	{
	case eF32:
		return new F32KeyTimesInformation(reinterpret_cast<f32*>(pData), numElements);

	case eUINT16:
		return new UINT16KeyTimesInformation(reinterpret_cast<uint16*>(pData), numElements);

	case eByte:
		return new ByteKeyTimesInformation(reinterpret_cast<uint8*>(pData), numElements);

	case eF32StartStop:
		return new F32SSKeyTimesInformation(reinterpret_cast<f32*>(pData), numElements);

	case eUINT16StartStop:
		return new UINT16SSKeyTimesInformation(reinterpret_cast<uint16*>(pData), numElements);

	case eByteStartStop:
		return new ByteSSKeyTimesInformation(reinterpret_cast<uint8*>(pData), numElements);

	case eBitset:
		return new CKeyTimesInformationBitSet(reinterpret_cast<uint16*>(pData), numElements);
	}
	return 0;
}

}                                                                                // namespace ControllerHelper

#if 0 // Currently unused, and needs support for modifiable controllers
void CCompressedController::UnCompress()
{

	CWaveletData Rotations[3];
	CWaveletData Positions[3];

	//	for (uint32 i = 0; i < 3; ++i)
	if (m_Rotations0.size() > 0)
	{
		Wavelet::UnCompress(m_Rotations0, Rotations[0]);
		Wavelet::UnCompress(m_Rotations1, Rotations[1]);
		Wavelet::UnCompress(m_Rotations2, Rotations[2]);

		uint32 size = m_pRotationController->GetKeyTimesInformation()->GetNumKeys();//Rotations[0].m_Data.size();

		//		m_pRotationController->Reserve(size);

		for (uint32 i = 0; i < size; ++i)
		{
			Ang3 ang(Rotations[0].m_Data[i], Rotations[1].m_Data[i], Rotations[2].m_Data[i]);
			Quat quat;
			quat.SetRotationXYZ(ang);
			quat.Normalize();
			m_pRotationController->GetRotationStorage()->AddValue(quat);
		}

	}

	if (m_Positions0.size() > 0)
	{
		Wavelet::UnCompress(m_Positions0, Positions[0]);
		Wavelet::UnCompress(m_Positions2, Positions[2]);
		Wavelet::UnCompress(m_Positions1, Positions[1]);

		uint32 size = m_pPositionController->GetKeyTimesInformation()->GetNumKeys(); //Positions[0].m_Data.size();
		//		m_pPositionController->Reserve(size);

		for (uint32 i = 0; i < size; ++i)
		{
			Vec3 pos(Positions[0].m_Data[i], Positions[1].m_Data[i], Positions[2].m_Data[i]);
			m_pPositionController->GetPositionStorage()->AddValue(pos);
		}

	}

	m_Rotations0.clear();
	m_Rotations1.clear();
	m_Rotations2.clear();
	m_Positions0.clear();
	m_Positions1.clear();
	m_Positions2.clear();

}
#endif

namespace ControllerHelper
{

const CRY_ALIGN(16) uint8 m_byteTable[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,1,  2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3,
	4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4, 2,3,  3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3,
	3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4,4,  5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3,
	4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4,5,  4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5,
	4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7, 2, 3, 3, 4,3,  4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5,
	6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5,6,  6, 7, 4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

const CRY_ALIGN(16) uint8 m_byteLowBit[256] = {
	8, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

const CRY_ALIGN(16) uint8 m_byteHighBit[256] = {
	16, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	4,  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
	5,  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	5,  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6,  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6,  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6,  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6,  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7,  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

} // namespace ControllerHelper

void IPositionController::GetValueFromKey(uint32 key, Vec3& val)
{
	uint32 numKeys = GetNumKeys();

	if (key >= numKeys)
	{
		key = numKeys - 1;
	}

	m_pData->GetValue(key, val);
}

void IRotationController::GetValueFromKey(uint32 key, Quat& val)
{
	uint32 numKeys = GetNumKeys();

	if (key >= numKeys)
	{
		key = numKeys - 1;
	}

	m_pData->GetValue(key, val);
}

JointState CControllerOptNonVirtual::GetOPS(f32 normalizedTime, Quat& quat, Vec3& pos, Diag33& scale) const
{
	return
	  CControllerOptNonVirtual::GetO(normalizedTime, quat) |
	  CControllerOptNonVirtual::GetP(normalizedTime, pos) |
	  CControllerOptNonVirtual::GetS(normalizedTime, scale);
}

JointState CControllerOptNonVirtual::GetOP(f32 normalized_time, Quat& quat, Vec3& pos) const
{
	return
	  CControllerOptNonVirtual::GetO(normalized_time, quat) |
	  CControllerOptNonVirtual::GetP(normalized_time, pos);
}

JointState CControllerOptNonVirtual::GetO(f32 normalized_time, Quat& quat) const
{
	if (eNoFormat == m_rotation.getTimeFormat())
		return 0;
	quat = this->CControllerOptNonVirtual::GetRotValue(normalized_time);
	return eJS_Orientation;
}

JointState CControllerOptNonVirtual::GetP(f32 normalizedTime, Vec3& pos) const
{
	if (eNoFormat == m_position.getTimeFormat())
		return 0;
	pos = this->CControllerOptNonVirtual::GetPosValue(normalizedTime);
	return eJS_Position;
}

JointState CControllerOptNonVirtual::GetS(f32 normalizedTime, Diag33& pos) const
{
	return 0;
}

JointState CController::GetOPS(f32 normalized_time, Quat& quat, Vec3& pos, Diag33& scale) const
{
	return
	  CController::GetO(normalized_time, quat) |
	  CController::GetP(normalized_time, pos) |
	  CController::GetS(normalized_time, scale);
}

JointState CController::GetOP(f32 normalized_time, Quat& quat, Vec3& pos) const
{
	return
	  CController::GetO(normalized_time, quat) |
	  CController::GetP(normalized_time, pos);
}

JointState CController::GetO(f32 normalized_time, Quat& quat) const
{
	if (m_pRotationController)
	{
		m_pRotationController->GetValue(normalized_time, quat);
		return eJS_Orientation;
	}

	return 0;
}

JointState CController::GetP(f32 normalized_time, Vec3& pos) const
{
	if (m_pPositionController)
	{
		m_pPositionController->GetValue(normalized_time, pos);
		return eJS_Position;
	}

	return 0;
}

JointState CController::GetS(f32 normalized_time, Diag33& scl) const
{
	if (m_pScaleController)
	{
		Vec3 tmp;
		m_pScaleController->GetValue(normalized_time, tmp);
		scl = Diag33(tmp);
		return eJS_Scale;
	}
	return 0;
}

int32 CController::GetO_numKey() const
{
	// TODO: Remove as a duplicated functionality (GetRotationKeysNum())
	if (m_pRotationController)
		return m_pRotationController->GetNumKeys();
	return -1;
}

int32 CController::GetP_numKey() const
{
	// TODO: Remove as a duplicated functionality (GetPositionKeysNum())
	if (m_pPositionController)
		return m_pPositionController->GetNumKeys();
	return -1;
}

size_t CController::GetRotationKeysNum() const
{
	if (m_pRotationController)
		return m_pRotationController->GetNumKeys();
	return 0;
}

size_t CController::GetPositionKeysNum() const
{
	if (m_pPositionController)
		return m_pPositionController->GetNumKeys();
	return 0;
}

size_t CController::GetScaleKeysNum() const
{
	if (m_pScaleController)
		return m_pScaleController->GetNumKeys();
	return 0;
}

size_t CController::SizeOfController() const
{
	size_t res = sizeof(*this);
	if (m_pPositionController)
		res += m_pPositionController->SizeOfPosController();
	if (m_pRotationController)
		res += m_pRotationController->SizeOfRotController();
	if (m_pScaleController)
		res += m_pScaleController->SizeOfPosController();
	return res;
}

size_t CController::ApproximateSizeOfThis() const
{
	return SizeOfController();
}

#if CRY_PLATFORM_SSE2 && !defined(_DEBUG)
__m128 SmallTree48BitQuat::div;
__m128 SmallTree48BitQuat::ran;

int Init48b = SmallTree48BitQuat::Init();
#endif
