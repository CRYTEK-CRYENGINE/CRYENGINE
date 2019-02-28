// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SkeletonData.h"
#include <cctype>

int SkeletonData::AddBone(void* handle, const char* name, int parentIndex)
{
	int modelIndex = int(m_bones.size());
	m_bones.push_back(BoneEntry(handle, name, parentIndex));
	m_nameBoneIndexMap.insert(std::make_pair(name, modelIndex));
	if (parentIndex >= 0)
		m_bones[parentIndex].children.push_back(modelIndex);
	else
		m_roots.push_back(modelIndex);
	return modelIndex;
}

int SkeletonData::FindBone(const char* name) const
{
	std::map<std::string, int>::const_iterator modelPos = m_nameBoneIndexMap.find(name);
	return (modelPos != m_nameBoneIndexMap.end() ? (*modelPos).second : -1);
}

void* SkeletonData::GetBoneHandle(int boneIndex) const
{
	return m_bones[boneIndex].handle;
}

int SkeletonData::GetBoneParentIndex(int boneIndex) const
{
	return m_bones[boneIndex].parentIndex;
}

int SkeletonData::GetBoneCount() const
{
	return int(m_bones.size());
}

void SkeletonData::SetTranslation(int modelIndex, const float* vec)
{
	for (int i = 0; i < 3; ++i)
		m_bones[modelIndex].translation[i] = vec[i];
}

void SkeletonData::SetRotation(int modelIndex, const float* vec)
{
	for (int i = 0; i < 3; ++i)
		m_bones[modelIndex].rotation[i] = vec[i];
}

void SkeletonData::SetScale(int modelIndex, const float* vec)
{
	for (int i = 0; i < 3; ++i)
		m_bones[modelIndex].scale[i] = vec[i];
}

void SkeletonData::SetParentFrameTranslation(int boneIndex, const float* vec)
{
	EnsureParentFrameExists(boneIndex);
	std::copy(vec, vec + 3, m_bones[boneIndex].parentFrameTranslation);
}

void SkeletonData::SetParentFrameRotation(int boneIndex, const float* vec)
{
	EnsureParentFrameExists(boneIndex);
	std::copy(vec, vec + 3, m_bones[boneIndex].parentFrameRotation);
}

void SkeletonData::SetParentFrameScale(int boneIndex, const float* vec)
{
	EnsureParentFrameExists(boneIndex);
	std::copy(vec, vec + 3, m_bones[boneIndex].parentFrameScale);
}

void SkeletonData::SetLimit(int boneIndex, Axis axis, Limit extreme, float limit)
{
	m_bones[boneIndex].limits.insert(std::make_pair(AxisLimit(axis, extreme), limit));
}

void SkeletonData::SetSpringTension(int boneIndex, Axis axis, float springTension)
{
	m_bones[boneIndex].springTensions.insert(std::make_pair(axis, springTension));
}

void SkeletonData::SetSpringAngle(int boneIndex, Axis axis, float springAngle)
{
	m_bones[boneIndex].springAngles.insert(std::make_pair(axis, springAngle));
}

void SkeletonData::SetAxisDamping(int boneIndex, Axis axis, float damping)
{
	m_bones[boneIndex].dampings.insert(std::make_pair(axis, damping));
}

void SkeletonData::SetPhysicalized(int boneIndex, bool physicalized)
{
	m_bones[boneIndex].physicalized = physicalized;
}

void SkeletonData::SetHasGeometry(int boneIndex, bool hasGeometry)
{
	m_bones[boneIndex].hasGeometry = hasGeometry;
}

void SkeletonData::SetBoneProperties(int boneIndex, const char* propertiesString )
{
	m_bones[boneIndex].propertiesString = propertiesString;
}

void SkeletonData::SetBoneGeomProperties(int boneIndex, const char* propertiesString )
{
	m_bones[boneIndex].geomPropertiesString = propertiesString;
}

bool SkeletonData::HasParentFrame(int boneIndex) const
{
	return m_bones[boneIndex].hasParentFrame;
}

void SkeletonData::GetParentFrameTranslation(int boneIndex, float* vec) const
{
	std::copy(m_bones[boneIndex].parentFrameTranslation, m_bones[boneIndex].parentFrameTranslation + 3, vec);
}

void SkeletonData::GetParentFrameRotation(int boneIndex, float* vec) const
{
	std::copy(m_bones[boneIndex].parentFrameRotation, m_bones[boneIndex].parentFrameRotation + 3, vec);
}

void SkeletonData::GetParentFrameScale(int boneIndex, float* vec) const
{
	std::copy(m_bones[boneIndex].parentFrameScale, m_bones[boneIndex].parentFrameScale + 3, vec);
}

bool SkeletonData::HasLimit(int boneIndex, Axis axis, Limit extreme) const
{
	return m_bones[boneIndex].limits.find(AxisLimit(axis, extreme)) != m_bones[boneIndex].limits.end();
}

float SkeletonData::GetLimit(int boneIndex, Axis axis, Limit extreme) const
{
	return (*m_bones[boneIndex].limits.find(AxisLimit(axis, extreme))).second;
}

bool SkeletonData::HasSpringTension(int boneIndex, Axis axis) const
{
	return m_bones[boneIndex].springTensions.find(axis) != m_bones[boneIndex].springTensions.end();
}

float SkeletonData::GetSpringTension(int boneIndex, Axis axis) const
{
	return (*m_bones[boneIndex].springTensions.find(axis)).second;
}

bool SkeletonData::HasSpringAngle(int boneIndex, Axis axis) const
{
	return m_bones[boneIndex].springAngles.find(axis) != m_bones[boneIndex].springAngles.end();
}

float SkeletonData::GetSpringAngle(int boneIndex, Axis axis) const
{
	return (*m_bones[boneIndex].springAngles.find(axis)).second;
}

bool SkeletonData::HasAxisDamping(int boneIndex, Axis axis) const
{
	return m_bones[boneIndex].dampings.find(axis) != m_bones[boneIndex].dampings.end();
}

float SkeletonData::GetAxisDamping(int boneIndex, Axis axis) const
{
	return (*m_bones[boneIndex].dampings.find(axis)).second;
}

bool SkeletonData::GetPhysicalized(int boneIndex) const
{
	return m_bones[boneIndex].physicalized;
}

bool SkeletonData::HasGeometry(int boneIndex) const
{
	return m_bones[boneIndex].hasGeometry;
}

int SkeletonData::GetRootCount() const
{
	return int(m_roots.size());
}

int SkeletonData::GetRootIndex(int rootIndex) const
{
	return m_roots[rootIndex];
}

int SkeletonData::GetParentIndex(int modelIndex) const
{
	return m_bones[modelIndex].parentIndex;
}

const std::string SkeletonData::GetName(int modelIndex) const
{
	std::string copy(m_bones[modelIndex].name);
	for (int i = 0, count = int(copy.size()); i < count; ++i)
	{
		if (!std::isalnum(copy[i]) && copy[i] != ' ')
			copy[i] = '_';
	}
	return copy;
}


const std::string SkeletonData::GetSafeName(int modelIndex) const
{
	std::string name = GetName(modelIndex);
	std::replace_if(name.begin(), name.end(), std::isspace, '_');
	return name;
}

int SkeletonData::GetChildCount(int modelIndex) const
{
	return int(m_bones[modelIndex].children.size());
}

int SkeletonData::GetChildIndex(int modelIndex, int childIndexIndex) const
{
	return m_bones[modelIndex].children[childIndexIndex];
}

void SkeletonData::GetTranslation(float* vec, int modelIndex) const
{
	for (int i = 0; i < 3; ++i)
		vec[i] = m_bones[modelIndex].translation[i];
}

void SkeletonData::GetRotation(float* vec, int modelIndex) const
{
	for (int i = 0; i < 3; ++i)
		vec[i] = m_bones[modelIndex].rotation[i];
}

void SkeletonData::GetScale(float* vec, int modelIndex) const
{
	for (int i = 0; i < 3; ++i)
		vec[i] = m_bones[modelIndex].scale[i];
}

const std::string SkeletonData::GetBoneProperties( int boneIndex ) const
{
	return m_bones[boneIndex].propertiesString;
}

const std::string SkeletonData::GetBoneGeomProperties( int boneIndex ) const
{
	return m_bones[boneIndex].geomPropertiesString;
}

void SkeletonData::EnsureParentFrameExists(int boneIndex)
{
	if (!m_bones[boneIndex].hasParentFrame)
	{
		std::fill(m_bones[boneIndex].parentFrameTranslation, m_bones[boneIndex].parentFrameTranslation + 3, 0.0f);
		std::fill(m_bones[boneIndex].parentFrameRotation, m_bones[boneIndex].parentFrameRotation + 3, 0.0f);
		std::fill(m_bones[boneIndex].parentFrameScale, m_bones[boneIndex].parentFrameScale + 3, 0.0f);
		m_bones[boneIndex].hasParentFrame = true;
	}
}

SkeletonData::BoneEntry::BoneEntry(void* handle, const std::string& name, int parentIndex)
	: handle(handle)
	, name(name)
	, parentIndex(parentIndex)
	, hasParentFrame(false)
	, physicalized(false)
	, hasGeometry(false)
{
	for (int i = 0; i < 3; ++i)
	{
		parentFrameTranslation[i] = 0.0f;
		parentFrameRotation[i] = 0.0f;
		parentFrameScale[i] = 1.0f;
		
		translation[i] = 0.0f;
		rotation[i] = 0.0f;
		scale[i] = 1.0f;
	}
}
