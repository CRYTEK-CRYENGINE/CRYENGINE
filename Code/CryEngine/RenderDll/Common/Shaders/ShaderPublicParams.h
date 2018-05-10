// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// IShaderPublicParams implementation class.
//////////////////////////////////////////////////////////////////////////
class CShaderPublicParams : public IShaderPublicParams
{
public:
	CShaderPublicParams() {}

	virtual void          SetParamCount(int nParam) { m_shaderParams.resize(nParam); }
	virtual int           GetParamCount() const { return m_shaderParams.size(); };

	virtual SShaderParam& GetParam(int nIndex);

	virtual const SShaderParam& GetParam(int nIndex) const;

	virtual SShaderParam* GetParamByName(const char* pszName);

	virtual const SShaderParam* GetParamByName(const char* pszName) const;

	virtual SShaderParam* GetParamBySemantic(uint8 eParamSemantic);

	virtual const SShaderParam* GetParamBySemantic(uint8 eParamSemantic) const;

	virtual void SetParam(int nIndex, const SShaderParam& param);

	virtual void AddParam(const SShaderParam& param);

	virtual void RemoveParamByName(const char* pszName);

	virtual void RemoveParamBySemantic(uint8 eParamSemantic);

	virtual void SetParam(const char* pszName, UParamVal& pParam, EParamType nType = eType_FLOAT, uint8 eSemantic = 0);

	virtual void SetShaderParams(const DynArray<SShaderParam>& pParams);

	virtual void AssignToRenderParams(struct SRendParams& rParams);

	virtual DynArray<SShaderParam>* GetShaderParams();

	virtual const DynArray<SShaderParam>* GetShaderParams() const;

	virtual uint8 GetSemanticByName(const char* szName);

private:
	DynArray<SShaderParam> m_shaderParams;
};
