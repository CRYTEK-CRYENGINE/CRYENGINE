// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelData.h"

int ModelData::AddModel(void* handle, const char* modelName, int parentModelIndex, bool geometry, const SHelperData& helperData, const std::string& propertiesString)
{
	int modelIndex = int(m_models.size());
	m_models.push_back(ModelEntry(handle, modelName, parentModelIndex, geometry, helperData, propertiesString));
	if (parentModelIndex >= 0)
		m_models[parentModelIndex].children.push_back(modelIndex);
	else
		m_roots.push_back(modelIndex);
	return modelIndex;
}

void* ModelData::GetModelHandle(int modelIndex) const
{
	return m_models[modelIndex].handle;
}

const char* ModelData::GetModelName(int modelIndex) const
{
	return m_models[modelIndex].name.c_str();
}

void ModelData::SetTranslationRotationScale(int const modelIndex, const float* const translation, const float* const rotation, const float* const scale)
{
	for (int i = 0; i < 3; ++i)
	{
		m_models[modelIndex].translation[i] = translation[i];
		m_models[modelIndex].rotation[i] = rotation[i];
		m_models[modelIndex].scale[i] = scale[i];
	}
}

void ModelData::GetTranslationRotationScale(int const modelIndex, float* const translation, float* const rotation, float* const scale) const
{
	for (int i = 0; i < 3; ++i)
	{
		translation[i] = m_models[modelIndex].translation[i];
		rotation[i] = m_models[modelIndex].rotation[i];
		scale[i] = m_models[modelIndex].scale[i];
	}
}

const SHelperData& ModelData::GetHelperData(int modelIndex) const
{
	return m_models[modelIndex].helperData;
}

const std::string& ModelData::GetProperties(int modelIndex) const
{
	return m_models[modelIndex].propertiesString;
}

int ModelData::GetParentModelIndex(int modelIndex) const
{
	return m_models[modelIndex].parentIndex;
}

int ModelData::GetModelCount() const
{
	return int(m_models.size());
}

int ModelData::GetRootCount() const
{
	return int(m_roots.size());
}

int ModelData::GetRootIndex(int rootIndex) const
{
	return m_roots[rootIndex];
}

int ModelData::GetChildCount(int modelIndex) const
{
	return int(m_models[modelIndex].children.size());
}

int ModelData::GetChildIndex(int modelIndex, int childIndexIndex) const
{
	return m_models[modelIndex].children[childIndexIndex];
}

bool ModelData::HasGeometry(int modelIndex) const
{
	return m_models[modelIndex].geometry;
}

ModelData::ModelEntry::ModelEntry(void* a_handle, const std::string& a_name, int a_parentIndex, bool a_geometry, const SHelperData& a_helperData, const std::string& a_propertiesString)
	: handle(a_handle)
	, name(a_name)
	, parentIndex(a_parentIndex)
	, geometry(a_geometry)
	, helperData(a_helperData)
	, propertiesString(a_propertiesString)
{
	translation[0] = translation[1] = translation[2] = 0.0f;
	rotation[0] = rotation[1] = rotation[2] = 0.0f;
	scale[0] = scale[1] = scale[2] = 1.0f;
}
