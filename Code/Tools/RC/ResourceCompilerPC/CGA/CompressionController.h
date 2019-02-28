// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "QuatQuantization.h"

class IController;
struct GlobalAnimationHeaderCAF;


struct ICompressedQuat
{
	virtual void FromQuat(const Quat& q) = 0;
	virtual Quat ToQuat() const = 0;
};

struct ICompressedVec3
{
	virtual void FromVec3(const Vec3& v) = 0;
	virtual Vec3 ToVec3() const = 0;
};

template<class _Realization>
struct CompressedQuatImpl : public ICompressedQuat
{
	virtual void FromQuat(const Quat& q)
	{	
		val.ToInternalType(q);
	}

	virtual Quat ToQuat() const
	{
		return val.ToExternalType();
	}

private:
	_Realization val;
};

typedef CompressedQuatImpl<NoCompressQuat> TNoCompressedQuat;
typedef CompressedQuatImpl<ShotInt3Quat> TShortInt3CompressedQuat;
typedef CompressedQuatImpl<SmallTreeDWORDQuat> TSmallTreeDWORDCompressedQuat;
typedef CompressedQuatImpl<SmallTree48BitQuat> TSmallTree48BitCompressedQuat;
typedef CompressedQuatImpl<SmallTree64BitQuat> TSmallTree64BitCompressedQuat;
typedef CompressedQuatImpl<SmallTree64BitExtQuat> TSmallTree64BitExtCompressedQuat;
typedef CompressedQuatImpl<PolarQuat> TPolarQuatQuat;

template<class _Realization>
struct CompressedVec3Impl : public ICompressedVec3
{
	virtual void FromVec3(const Vec3& q)
	{	
		val.ToInternalType(q);
	}

	virtual Vec3 ToVec3() const
	{
		return val.ToExternalType();
	}

private:
	_Realization val;
};

typedef CompressedVec3Impl<NoCompressVec3> TBaseCompressedVec3;


class CCompressonator
{

public:

	std::vector<Quat> m_rotations;
	std::vector<Vec3> m_positions;
	std::vector<Diag33> m_scales;

	std::vector<int> m_rotTimes;
	std::vector<int> m_posTimes;
	std::vector<int> m_sclTimes;

	void CompressAnimation(
		const GlobalAnimationHeaderCAF& header,
		GlobalAnimationHeaderCAF& newheader,
		const CSkeletonInfo * skeleton,
		const SPlatformAnimationSetup& platform,
		const SAnimationDesc& desc,
		bool bDebugCompression);

	RotationControllerPtr CompressRotations(ECompressionFormat rotationFormat, bool bRemoveKeys, float rotationToleranceInDegrees);
	PositionControllerPtr CompressPositions(ECompressionFormat positionFormat, bool bRemoveKeys, float positionTolerance);
	PositionControllerPtr CompressScales(ECompressionFormat scaleFormat, bool shouldRemoveKyes, float scaleTolerance);

private:

	enum EOperation
	{
		eOperation_DontCompress,  // Used exclusively by the 'old format' (set when desc.oldFmt.m_CompressionQuality == 0).
		eOperation_Compress,      // Used in most cases.
		eOperation_Delete         // Set by the 'Compute*' family methods to determine tracks that can be safely removed.
	};

	struct Operations
	{
		EOperation m_ePosOperation;
		EOperation m_eRotOperation;
		EOperation m_eSclOperation;
	};

	// TODO: The following methods contain lots and lots of duplicated code. We might want to merge them together at some point.

	bool ComputeControllerDelete_Override(
		Operations& operations,
		const GlobalAnimationHeaderCAF& header,
		const CSkeletonInfo* pSkeleton,
		uint32 controllerIndex,
		const SPlatformAnimationSetup& platform,
		const SAnimationDesc& desc,
		bool bDebugCompression); // TODO: Use a verbosity flag instead of passing this around.

	bool CompressController_Override(
		const Operations& operations,
		const GlobalAnimationHeaderCAF& header,
		GlobalAnimationHeaderCAF& newheader,
		const CSkeletonInfo* pSkeleton,
		uint32 controllerIndex,
		const SPlatformAnimationSetup& platform,
		const SAnimationDesc& desc,
		bool bDebugCompression);

	bool ComputeControllerDelete_Additive(
		Operations& operations,
		const GlobalAnimationHeaderCAF& header,
		const CSkeletonInfo* pSkeleton,
		uint32 controllerIndex,
		const SPlatformAnimationSetup& platform,
		const SAnimationDesc& desc,
		bool bDebugCompression);

	bool CompressController_Additive(
		const Operations& operations,
		const GlobalAnimationHeaderCAF& header,
		GlobalAnimationHeaderCAF& newheader,
		const CSkeletonInfo* pSkeleton,
		uint32 controllerIndex,
		const SPlatformAnimationSetup& platform,
		const SAnimationDesc& desc,
		bool bDebugCompression);

	void CompressController(
		float positionTolerance,
		float rotationToleranceInDegrees,
		float scaleTolerance,
		const Operations& operations,
		GlobalAnimationHeaderCAF& newheader,
		uint32 controllerIndex,
		uint32 controllerId);
	 
	// Computes a base frame which should be subtracted from the currently processed controller when generating additive animation.
	QuatTNS ComputeAdditiveBaseFrame(const GlobalAnimationHeaderCAF& header, const uint32 controllerId) const;
};


// This function is copy of pre-March2012 version of Quat::IsEquivalent().
// (Changes made in Quat::IsEquivalent() broke binary compatibility, so now 
// we are forced to use our own copy of the function).
ILINE bool IsQuatEquivalent_dot(const Quat& q0, const Quat& q1, float e)
{
	const Quat p1 = -q1;
	const bool t0 = (fabs_tpl(q0.v.x-q1.v.x)<=e) && (fabs_tpl(q0.v.y-q1.v.y)<=e) && (fabs_tpl(q0.v.z-q1.v.z)<=e) && (fabs_tpl(q0.w-q1.w)<=e);
	const bool t1 = (fabs_tpl(q0.v.x-p1.v.x)<=e) && (fabs_tpl(q0.v.y-p1.v.y)<=e) && (fabs_tpl(q0.v.z-p1.v.z)<=e) && (fabs_tpl(q0.w-p1.w)<=e);
	return t0 || t1;
}
