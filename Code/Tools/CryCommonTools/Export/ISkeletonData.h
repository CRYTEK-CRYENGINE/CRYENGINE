// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ISKELETONDATA_H__
#define __ISKELETONDATA_H__

class ISkeletonData
{
public:
	enum Axis
	{
		AxisX,
		AxisY,
		AxisZ
	};
	enum Limit
	{
		LimitMin,
		LimitMax
	};

	virtual int AddBone(void* handle, const char* name, int parentIndex) = 0;
	virtual int FindBone(const char* name) const = 0;
	virtual void* GetBoneHandle(int boneIndex) const = 0;
	virtual int GetBoneParentIndex(int boneIndex) const = 0;
	virtual int GetBoneCount() const = 0;
	virtual void SetTranslation(int boneIndex, const float* vec) = 0;
	virtual void SetRotation(int boneIndex, const float* vec) = 0;
	virtual void SetScale(int boneIndex, const float* vec) = 0;
	virtual void SetParentFrameTranslation(int boneIndex, const float* vec) = 0;
	virtual void SetParentFrameRotation(int boneIndex, const float* vec) = 0;
	virtual void SetParentFrameScale(int boneIndex, const float* vec) = 0;
	virtual void SetPhysicalized(int boneIndex, bool physicalized) = 0;
	virtual void SetHasGeometry(int boneIndex, bool hasGeometry) = 0;
	virtual void SetBoneProperties(int boneIndex, const char* propertiesString ) = 0;
	virtual void SetBoneGeomProperties(int boneIndex, const char* propertiesString ) = 0;

	virtual void SetLimit(int boneIndex, Axis axis, Limit extreme, float limit) = 0;
	virtual void SetSpringTension(int boneIndex, Axis axis, float springTension) = 0;
	virtual void SetSpringAngle(int boneIndex, Axis axis, float springAngle) = 0;
	virtual void SetAxisDamping(int boneIndex, Axis axis, float damping) = 0;
};

#endif //__ISKELETONDATA_H__
