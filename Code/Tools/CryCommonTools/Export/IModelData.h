// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __IMODELDATA_H__
#define __IMODELDATA_H__

#include "HelperData.h"
#include <string>

class IModelData
{
public:
	virtual int AddModel(void* handle, const char* modelName, int parentModelIndex, bool geometry, const SHelperData& helperData, const std::string& propertiesString) = 0;
	virtual int GetModelCount() const = 0;
	virtual void* GetModelHandle(int modelIndex) const = 0;
	virtual const char* GetModelName(int modelIndex) const = 0;
	virtual void SetTranslationRotationScale(int modelIndex, const float* translation, const float* rotation, const float* scale) = 0;
	virtual void GetTranslationRotationScale(int modelIndex, float* translation, float* rotation, float* scale) const = 0;
	virtual const SHelperData& GetHelperData(int modelIndex) const = 0;
	virtual const std::string& GetProperties(int modelIndex) const = 0;
	virtual int GetParentModelIndex(int modelIndex) const = 0;
};

#endif //__IMODELDATA_H__
