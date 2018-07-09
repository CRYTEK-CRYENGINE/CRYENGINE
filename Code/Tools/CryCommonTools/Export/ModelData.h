// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MODELDATA_H__
#define __MODELDATA_H__

#include "IModelData.h"

class ModelData : public IModelData
{
public:
	// IModelData
	virtual int AddModel(void* handle, const char* name, int parentModelIndex, bool geometry, const SHelperData& helperData, const std::string& propertiesString);
	virtual int GetModelCount() const;
	virtual void* GetModelHandle(int modelIndex) const;
	virtual const char* GetModelName(int modelIndex) const;
	virtual void SetTranslationRotationScale(int modelIndex, const float* translation, const float* rotation, const float* scale);
	virtual void GetTranslationRotationScale(int modelIndex, float* translation, float* rotation, float* scale) const;
	virtual const SHelperData& GetHelperData(int modelIndex) const;
	virtual const std::string& GetProperties(int modelIndex) const;
	virtual int GetParentModelIndex(int modelIndex) const;

	int GetRootCount() const;
	int GetRootIndex(int rootIndex) const;
	int GetChildCount(int modelIndex) const;
	int GetChildIndex(int modelIndex, int childIndexIndex) const;
	bool HasGeometry(int modelIndex) const;

private:
	struct ModelEntry
	{
		ModelEntry(void* handle, const std::string& name, int parentIndex, bool geometry, const SHelperData& helperData, const std::string& propertiesString);

		void* handle;
		std::string name;
		int parentIndex;
		bool geometry;
		std::vector<int> children;
		float translation[3];
		float rotation[3];
		float scale[3];
		SHelperData helperData;
		std::string propertiesString;
	};

	std::vector<ModelEntry> m_models;
	std::vector<int> m_roots;
};

#endif //__MODELDATA_H__
