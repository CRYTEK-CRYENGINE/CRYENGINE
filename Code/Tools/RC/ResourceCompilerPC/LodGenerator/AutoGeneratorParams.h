// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace LODGenerator 
{
	class CAutoGeneratorParams
	{
	public:
		CAutoGeneratorParams();
		~CAutoGeneratorParams();

		template <class T> 
		T GetGeometryOption(CString paramName)
		{
			T value;
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->Get(value);
			return value;
		}

		template <class T> 
		void SetGeometryOption(CString paramName, T paramValue)
		{
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->Set(paramValue);
		}

		template <class T> 
		T GetMaterialOption(CString paramName)
		{
			T value;
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->Get(value);
			return value;
		}

		template <class T> 
		void SetMaterialOption(CString paramName, T paramValue)
		{
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->Set(paramValue);
		}

		void SetGeometryOptionLimit(CString paramName, float fMin, float fMax, float fStep = 0.f, bool bHardMin = true, bool bHardMax = true )
		{
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->SetLimits(fMin,fMax,fStep,bHardMin,bHardMax);
		}

		void SetMaterialOptionLimit(CString paramName, float fMin, float fMax, float fStep = 0.f, bool bHardMin = true, bool bHardMax = true )
		{
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->SetLimits(fMin,fMax,fStep,bHardMin,bHardMax);
		}

		void GetGeometryOptionLimit(CString paramName, float& fMin, float& fMax, float& fStep, bool& bHardMin, bool& bHardMax )
		{
			if (IVariable* pVar = m_pGeometryVarBlock->FindVariable(paramName))
				pVar->GetLimits(fMin,fMax,fStep,bHardMin,bHardMax);
		}

		void GetMaterialOptionLimit(CString paramName, float& fMin, float& fMax, float& fStep, bool& bHardMin, bool& bHardMax )
		{
			if (IVariable* pVar = m_pMaterialVarBlock->FindVariable(paramName))
				pVar->GetLimits(fMin,fMax,fStep,bHardMin,bHardMax);
		}

		void EnableUpdateCallbacks(bool enable);

	private:
		_smart_ptr<CVarBlock>		m_pGeometryVarBlock;
		CSmartVariable<int>			m_nSourceLod;
		CSmartVariable<bool>		m_bObjectHasBase;
		CSmartVariable<int>			m_nViewsAround;
		CSmartVariable<int>			m_nViewElevations;
		CSmartVariable<float>		m_fSilhouetteWeight;
		CSmartVariable<float>		m_fViewResolution;
		CSmartVariable<float>		m_fVertexWelding;
		CSmartVariable<bool>		m_bCheckTopology;
		CSmartVariable<bool>		m_bWireframe;
		CSmartVariable<bool>		m_bExportObj;
		CSmartVariable<bool>		m_bAddToParentMaterial;
		CSmartVariable<bool>		m_bUseCageMesh;
		CSmartVariable<bool>		m_bPreviewSourceLod;

		_smart_ptr<CVarBlock>		m_pMaterialVarBlock;
		CSmartVariable<float>		m_fRayLength;
		CSmartVariable<float>		m_fRayStartDist;
		CSmartVariable<bool>		m_bBakeAlpha;
		CSmartVariable<bool>		m_bBakeSpec;
		CSmartVariable<bool>		m_bSmoothCage;
		CSmartVariable<bool>		m_bDilationPass;
		CSmartVariable<int>			m_cBackgroundColour;
		CSmartVariable<CString>		m_strExportPath;
		CSmartVariable<CString>		m_strFilename;
		CSmartVariable<CString>		m_strPresetDiffuse;
		CSmartVariable<CString>		m_strPresetNormal;
		CSmartVariable<CString>		m_strPresetSpecular;
		CSmartVariable<bool>		m_useAutoTextureSize;
		CSmartVariable<float>		m_autoTextureRadius1;
		CSmartVariable<int>			m_autoTextureSize1;
		CSmartVariable<float>		m_autoTextureRadius2;
		CSmartVariable<int>			m_autoTextureSize2;
		CSmartVariable<float>		m_autoTextureRadius3;
		CSmartVariable<int>			m_autoTextureSize3;
	};

}