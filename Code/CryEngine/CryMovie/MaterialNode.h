// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MaterialNode_h__
#define __MaterialNode_h__
#pragma once

#include "AnimNode.h"

class CAnimMaterialNode : public CAnimNode
{
public:
	CAnimMaterialNode(const int id);
	static void            Initialize();

	virtual void           SetName(const char* name) override;

	virtual EAnimNodeType  GetType() const override { return eAnimNodeType_Material; }

	virtual void           Animate(SAnimContext& animContext) override;
	virtual unsigned int   GetParamCount() const override;
	virtual CAnimParamType GetParamType(unsigned int nIndex) const override;
	virtual const char*    GetParamName(const CAnimParamType& paramType) const override;

	virtual void           InitializeTrackDefaultValue(IAnimTrack* pTrack, const CAnimParamType& paramType) override;

	virtual void           UpdateDynamicParams() override;

protected:
	virtual bool GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const override;

private:
	void       SetScriptValue();
	void       AnimateNamedParameter(SAnimContext& animContext, IRenderShaderResources* pShaderResources, const char* name, IAnimTrack* pTrack);
	IMaterial* GetMaterialByName(const char* pName);

	std::vector<CAnimNode::SParamInfo> m_dynamicShaderParamInfos;
	typedef std::unordered_map<string, size_t, stl::hash_stricmp<string>, stl::hash_stricmp<string>> TDynamicShaderParamsMap;
	TDynamicShaderParamsMap            m_nameToDynamicShaderParam;
};

#endif // __MaterialNode_h__
