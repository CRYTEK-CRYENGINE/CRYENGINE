// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimPostFXNode.h"
#include "AnimSplineTrack.h"
#include "CompoundSplineTrack.h"
#include "BoolTrack.h"

#include <Cry3DEngine/I3DEngine.h>

class CFXNodeDescription : public _i_reference_target_t
{
public:
	class CControlParamBase : public _i_reference_target_t
	{
	public:
		CControlParamBase() {}

		virtual void SetDefault(float val) = 0;
		virtual void SetDefault(bool val) = 0;
		virtual void SetDefault(Vec4 val) = 0;

		virtual void GetDefault(float& val) const = 0;
		virtual void GetDefault(bool& val) const = 0;
		virtual void GetDefault(Vec4& val) const = 0;

		string m_name;

	protected:
		virtual ~CControlParamBase() {}
	};

	template<typename T>
	class TControlParam : public CControlParamBase
	{
	public:
		virtual void SetDefault(float val)        { assert(0); }
		virtual void SetDefault(bool val)         { assert(0); }
		virtual void SetDefault(Vec4 val)         { assert(0); }

		virtual void GetDefault(float& val) const { assert(0); }
		virtual void GetDefault(bool& val) const  { assert(0); }
		virtual void GetDefault(Vec4& val) const  { assert(0); }

	protected:
		virtual ~TControlParam() {}

		T m_defaultValue;
	};

	CFXNodeDescription(EAnimNodeType nodeType) : m_nodeType(nodeType) {}

	template<typename T>
	void AddSupportedParam(const char* sName, EAnimValue eValueType, const char* sControlName, T defaultValue)
	{
		CAnimNode::SParamInfo param;
		param.name = sName;
		param.paramType = eAnimParamType_User + (int)m_nodeParams.size();
		param.valueType = eValueType;
		m_nodeParams.push_back(param);

		TControlParam<T>* control = new TControlParam<T>;
		control->m_name = sControlName;
		control->SetDefault(defaultValue);

		m_controlParams.push_back(control);
	}

	EAnimNodeType                              m_nodeType;
	std::vector<CAnimNode::SParamInfo>         m_nodeParams;
	std::vector<_smart_ptr<CControlParamBase>> m_controlParams;
};

template<> void CFXNodeDescription::TControlParam<float >::SetDefault(float val)
{
	m_defaultValue = val;
}

template<> void CFXNodeDescription::TControlParam<bool >::SetDefault(bool val)
{
	m_defaultValue = val;
}

template<> void CFXNodeDescription::TControlParam<Vec4 >::SetDefault(Vec4 val)
{
	m_defaultValue = val;
}

template<> void CFXNodeDescription::TControlParam<float >::GetDefault(float& val) const
{
	val = m_defaultValue;
}

template<> void CFXNodeDescription::TControlParam<bool >::GetDefault(bool& val) const
{
	val = m_defaultValue;
}

template<> void CFXNodeDescription::TControlParam<Vec4 >::GetDefault(Vec4& val) const
{
	val = m_defaultValue;
}

CAnimPostFXNode::FxNodeDescriptionMap CAnimPostFXNode::s_fxNodeDescriptions;
bool CAnimPostFXNode::s_initialized = false;

CAnimPostFXNode::CAnimPostFXNode(const int id, CFXNodeDescription* pDesc)
	: CAnimNode(id), m_pDescription(pDesc)
{
}

void CAnimPostFXNode::Initialize()
{
	if (!s_initialized)
	{
		s_initialized = true;

		//! Radial Blur
		{
			CFXNodeDescription* pDesc = new CFXNodeDescription(eAnimNodeType_RadialBlur);
			s_fxNodeDescriptions[eAnimNodeType_RadialBlur] = pDesc;
			pDesc->m_nodeParams.reserve(4);
			pDesc->m_controlParams.reserve(4);
			pDesc->AddSupportedParam<float>("Amount", eAnimValue_Float, "FilterRadialBlurring_Amount", 0.0f);
			pDesc->AddSupportedParam<float>("ScreenPosX", eAnimValue_Float, "FilterRadialBlurring_ScreenPosX", 0.5f);
			pDesc->AddSupportedParam<float>("ScreenPosY", eAnimValue_Float, "FilterRadialBlurring_ScreenPosY", 0.5f);
			pDesc->AddSupportedParam<float>("BlurringRadius", eAnimValue_Float, "FilterRadialBlurring_Radius", 1.0f);
		}

		//! Color Correction
		{
			CFXNodeDescription* pDesc = new CFXNodeDescription(eAnimNodeType_ColorCorrection);
			s_fxNodeDescriptions[eAnimNodeType_ColorCorrection] = pDesc;
			pDesc->m_nodeParams.reserve(8);
			pDesc->m_controlParams.reserve(8);
			pDesc->AddSupportedParam<float>("Cyan", eAnimValue_Float, "Global_User_ColorC", 0.0f);
			pDesc->AddSupportedParam<float>("Magenta", eAnimValue_Float, "Global_User_ColorM", 0.0f);
			pDesc->AddSupportedParam<float>("Yellow", eAnimValue_Float, "Global_User_ColorY", 0.0f);
			pDesc->AddSupportedParam<float>("Luminance", eAnimValue_Float, "Global_User_ColorK", 0.0f);
			pDesc->AddSupportedParam<float>("Brightness", eAnimValue_Float, "Global_User_Brightness", 1.0f);
			pDesc->AddSupportedParam<float>("Contrast", eAnimValue_Float, "Global_User_Contrast", 1.0f);
			pDesc->AddSupportedParam<float>("Saturation", eAnimValue_Float, "Global_User_Saturation", 1.0f);
			pDesc->AddSupportedParam<float>("Hue", eAnimValue_Float, "Global_User_ColorHue", 0.0f);
		}

		//! Depth of Field
		{
			CFXNodeDescription* pDesc = new CFXNodeDescription(eAnimNodeType_DepthOfField);
			s_fxNodeDescriptions[eAnimNodeType_DepthOfField] = pDesc;
			pDesc->m_nodeParams.reserve(4);
			pDesc->m_controlParams.reserve(4);
			pDesc->AddSupportedParam<bool>("Enable", eAnimValue_Bool, "Dof_User_Active", false);
			pDesc->AddSupportedParam<float>("FocusDistance", eAnimValue_Float, "Dof_User_FocusDistance", 3.5f);
			pDesc->AddSupportedParam<float>("FocusRange", eAnimValue_Float, "Dof_User_FocusRange", 5.0f);
			pDesc->AddSupportedParam<float>("BlurAmount", eAnimValue_Float, "Dof_User_BlurAmount", 1.0f);
		}

		//! HDR setup - expose couple controls to cinematics
		{
			CFXNodeDescription* pDesc = new CFXNodeDescription(eAnimNodeType_HDRSetup);
			s_fxNodeDescriptions[eAnimNodeType_HDRSetup] = pDesc;
			pDesc->m_nodeParams.reserve(3);
			pDesc->m_controlParams.reserve(3);
			pDesc->AddSupportedParam<float>("HDRBrightLevel", eAnimValue_Float, "Global_User_HDRBrightLevel", 0.0f);
			pDesc->AddSupportedParam<float>("HDRBrightThreshold", eAnimValue_Float, "Global_User_HDRBrightThreshold", 0.0f);
			pDesc->AddSupportedParam<float>("HDRExposureFactor", eAnimValue_Float, "Global_User_HDRExposureFactor", 1.0f);
		}

		//! Shadow setup - expose couple shadow controls to cinematics
		{
			CFXNodeDescription* pDesc = new CFXNodeDescription(eAnimNodeType_ShadowSetup);
			s_fxNodeDescriptions[eAnimNodeType_ShadowSetup] = pDesc;
			pDesc->m_nodeParams.reserve(1);
			pDesc->m_controlParams.reserve(1);
			pDesc->AddSupportedParam<float>("GSMCache", eAnimValue_Bool, "GSMCacheParam", true);
		}
	}
}

CAnimNode* CAnimPostFXNode::CreateNode(const int id, EAnimNodeType nodeType)
{
	CAnimPostFXNode::Initialize();

	FxNodeDescriptionMap::iterator itr = s_fxNodeDescriptions.find(nodeType);

	if (itr == s_fxNodeDescriptions.end())
	{
		return 0;
	}

	CFXNodeDescription* pDesc = itr->second;

	return new CAnimPostFXNode(id, pDesc);
}

void CAnimPostFXNode::SerializeAnims(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	if (bLoading)
	{
		int paramIdVersion = 0;
		xmlNode->getAttr("ParamIdVersion", paramIdVersion);

		// Fix old param types
		if (paramIdVersion <= 2)
		{
			int num = xmlNode->getChildCount();

			for (int i = 0; i < num; ++i)
			{
				XmlNodeRef trackNode = xmlNode->getChild(i);

				CAnimParamType paramType;
				paramType.Serialize(trackNode, true);
				// Don't use APARAM_USER because it could change in newer versions
				// CAnimNode::SerializeAnims will then take care of that
				static const unsigned int OLD_APARAM_USER = 100;
				paramType = paramType.GetType() + OLD_APARAM_USER;
				paramType.Serialize(trackNode, false);
			}
		}
	}

	CAnimNode::SerializeAnims(xmlNode, bLoading, bLoadEmptyTracks);
}

EAnimNodeType CAnimPostFXNode::GetType() const
{
	return m_pDescription->m_nodeType;
}

unsigned int CAnimPostFXNode::GetParamCount() const
{
	return m_pDescription->m_nodeParams.size();
}

CAnimParamType CAnimPostFXNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < m_pDescription->m_nodeParams.size())
	{
		return m_pDescription->m_nodeParams[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimPostFXNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < m_pDescription->m_nodeParams.size(); ++i)
	{
		if (m_pDescription->m_nodeParams[i].paramType == paramId)
		{
			info = m_pDescription->m_nodeParams[i];
			return true;
		}
	}

	return false;
}

void CAnimPostFXNode::CreateDefaultTracks()
{
	for (size_t i = 0; i < m_pDescription->m_nodeParams.size(); ++i)
	{
		IAnimTrack* pTrack = CreateTrackInternal(m_pDescription->m_nodeParams[i].paramType, m_pDescription->m_nodeParams[i].valueType);

		//Setup default value
		EAnimValue valueType = m_pDescription->m_nodeParams[i].valueType;

		if (valueType == eAnimValue_Float)
		{
			CAnimSplineTrack* pFloatTrack = static_cast<CAnimSplineTrack*>(pTrack);
			float val(0);
			m_pDescription->m_controlParams[i]->GetDefault(val);
			pFloatTrack->SetDefaultValue(TMovieSystemValue(val));
		}
		else if (valueType == eAnimValue_Bool)
		{
			CBoolTrack* pBoolTrack = static_cast<CBoolTrack*>(pTrack);
			bool val = false;
			m_pDescription->m_controlParams[i]->GetDefault(val);
			pBoolTrack->SetDefaultValue(TMovieSystemValue(val));
		}
		else if (valueType == eAnimValue_Vector4)
		{
			CCompoundSplineTrack* pCompoundTrack = static_cast<CCompoundSplineTrack*>(pTrack);
			Vec4 val(0.0f, 0.0f, 0.0f, 0.0f);
			m_pDescription->m_controlParams[i]->GetDefault(val);
			pCompoundTrack->SetDefaultValue(TMovieSystemValue(val));
		}
	}
}

void CAnimPostFXNode::Animate(SAnimContext& animContext)
{
	for (size_t i = 0; i < m_tracks.size(); ++i)
	{
		IAnimTrack* pTrack = m_tracks[i];
		assert(pTrack);
		size_t paramIndex = (size_t)m_tracks[i]->GetParameterType().GetType() - eAnimParamType_User;
		assert(paramIndex < m_pDescription->m_nodeParams.size());
		bool useDefaultValue = (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) || pTrack->IsMasked(animContext.trackMask);

		EAnimValue valueType = m_pDescription->m_nodeParams[paramIndex].valueType;
		const TMovieSystemValue value = pTrack->GetValue(animContext.time);

		// sorry: quick & dirty solution for c2 shipping - custom type handling for shadows - make this properly after shipping
		if (GetType() == eAnimNodeType_ShadowSetup && valueType == eAnimValue_Bool)
		{
			bool val(false);
			if (useDefaultValue)
				m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			else
				val = boost::get<bool>(value);

			gEnv->p3DEngine->SetShadowsGSMCache(val);
		}
		else if (valueType == eAnimValue_Float)
		{
			float val(0.0f);
			if (useDefaultValue)
				m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			else
				val = boost::get<float>(value);

			gEnv->p3DEngine->SetPostEffectParam(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), val);
		}
		else if (valueType == eAnimValue_Bool)
		{
			bool val(false);
			if (useDefaultValue)
				m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			else
				val = boost::get<bool>(value);

			gEnv->p3DEngine->SetPostEffectParam(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), (val ? 1.f : 0.f));
		}
		else if (valueType == eAnimValue_Vector4)
		{
			Vec4 val(0.0f, 0.0f, 0.0f, 0.0f);
			if (useDefaultValue)
				m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			else
				val = boost::get<Vec4>(value);

			gEnv->p3DEngine->SetPostEffectParamVec4(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), val);
		}
	}
}

void CAnimPostFXNode::Activate(bool activate)
{
	CAnimNode::Activate(activate);

	if (activate)
		return;

	// Reset each postFX param to its default.
	for (size_t i = 0; i < m_tracks.size(); ++i)
	{
		IAnimTrack* pTrack = m_tracks[i];
		assert(pTrack);
		size_t paramIndex = (size_t)m_tracks[i]->GetParameterType().GetType() - eAnimParamType_User;
		assert(paramIndex < m_pDescription->m_nodeParams.size());

		EAnimValue valueType = m_pDescription->m_nodeParams[paramIndex].valueType;

		// sorry: quick & dirty solution for c2 shipping - custom type handling for shadows - make this properly after shipping
		if (GetType() == eAnimNodeType_ShadowSetup && valueType == eAnimValue_Bool)
		{
			bool val(false);
			m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			gEnv->p3DEngine->SetShadowsGSMCache(val);
		}
		else if (valueType == eAnimValue_Float)
		{
			float val(0);
			m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			gEnv->p3DEngine->SetPostEffectParam(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), val);
		}
		else if (valueType == eAnimValue_Bool)
		{
			bool val(false);
			m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			gEnv->p3DEngine->SetPostEffectParam(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), (val ? 1.f : 0.f));
		}
		else if (valueType == eAnimValue_Vector4)
		{
			Vec4 val(0.0f, 0.0f, 0.0f, 0.0f);
			m_pDescription->m_controlParams[paramIndex]->GetDefault(val);
			gEnv->p3DEngine->SetPostEffectParamVec4(m_pDescription->m_controlParams[paramIndex]->m_name.c_str(), val);
		}
	}
}
