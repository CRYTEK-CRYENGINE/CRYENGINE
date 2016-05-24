// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimScreenFaderNode.h"
#include "ScreenFaderTrack.h"
#include <CryRenderer/IRenderer.h>

namespace
{
bool s_screenFaderNodeParamsInitialized = false;
std::vector<CAnimNode::SParamInfo> s_screenFaderNodeParams;

void AddSupportedParams(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	param.flags = IAnimNode::eSupportedParamFlags_MultipleTracks;
	s_screenFaderNodeParams.push_back(param);
}
};

bool CalculateIsolatedKeyColor(const SScreenFaderKey& key, SAnimTime time, Vec4& colorOut)
{
	SAnimTime ratio = time - key.m_time;

	if (ratio < SAnimTime(0))
	{
		return false;
	}

	if (key.m_fadeTime == 0.f)
	{
		colorOut = key.m_fadeColor;

		if (key.m_fadeType == SScreenFaderKey::eFT_FadeIn)
		{
			colorOut.w = 0.f;
		}
		else
		{
			colorOut.w = 1.f;
		}
	}
	else
	{
		colorOut = key.m_fadeColor;
		float floatRatio = ratio.ToFloat() / key.m_fadeTime;

		if (key.m_fadeType == SScreenFaderKey::eFT_FadeIn)
		{
			colorOut.w = MAX(0.f, 1.f - floatRatio);
		}
		else
		{
			colorOut.w = MIN(1.f, floatRatio);
		}
	}

	return true;
}

CAnimScreenFaderNode::CAnimScreenFaderNode(const int id)
	: CAnimNode(id), m_bActive(false), m_screenWidth(800.f), m_screenHeight(600.f), m_lastActivatedKey(-1), m_texPrecached(false)
{
	m_startColor = Vec4(1, 1, 1, 1);
	CAnimScreenFaderNode::Initialize();
	PrecacheTexData();
}

CAnimScreenFaderNode::~CAnimScreenFaderNode()
{
}

void CAnimScreenFaderNode::Initialize()
{
	if (!s_screenFaderNodeParamsInitialized)
	{
		s_screenFaderNodeParamsInitialized = true;
		s_screenFaderNodeParams.reserve(1);
		AddSupportedParams("Fader", eAnimParamType_ScreenFader, eAnimValue_Unknown);
	}
}

void CAnimScreenFaderNode::Animate(SAnimContext& animContext)
{
	size_t const nScreenFaderTracksNumber = m_tracks.size();

	for (size_t nFaderTrackNo = 0; nFaderTrackNo < nScreenFaderTracksNumber; ++nFaderTrackNo)
	{
		CScreenFaderTrack* pTrack = static_cast<CScreenFaderTrack*>(GetTrackForParameter(eAnimParamType_ScreenFader, nFaderTrackNo));

		if (!pTrack || pTrack->GetNumKeys() == 0 || (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) != 0 || pTrack->IsMasked(animContext.trackMask))
		{
			continue;
		}

		if (animContext.bSingleFrame)
		{
			m_lastActivatedKey = -1;
		}

		SScreenFaderKey key;
		int nActiveKeyIndex = pTrack->GetActiveKey(animContext.time, &key);

		if (nActiveKeyIndex >= 0)
		{
			if (m_lastActivatedKey != nActiveKeyIndex)
			{
				m_lastActivatedKey = nActiveKeyIndex;
				m_bActive = true;

				if (key.m_texture[0])
				{
					if (pTrack->SetActiveTexture(nActiveKeyIndex))
					{
						pTrack->SetTextureVisible(true);
					}
					else
					{
						pTrack->SetTextureVisible(false);
					}
				}
				else
				{
					pTrack->SetTextureVisible(false);
				}
			}

			if (m_bActive || SAnimTime(key.m_fadeTime) + key.m_time > animContext.time)
			{
				float ratio = (key.m_fadeTime > 0) ? ((animContext.time - key.m_time) / key.m_fadeTime).ToFloat() : 1.f;

				if (ratio < 0.f) { ratio = 0.f; }

				ratio = MIN(ratio, 1.f);

				switch (key.m_fadeChangeType)
				{
				case SScreenFaderKey::eFCT_Square:
					ratio = ratio * ratio;
					break;

				case SScreenFaderKey::eFCT_CubicSquare:
					ratio = ratio * ratio * ratio;
					break;

				case SScreenFaderKey::eFCT_SquareRoot:
					ratio = sqrt(ratio);
					break;

				case SScreenFaderKey::eFCT_Sin:
					ratio = sinf(ratio * 3.14159265f * 0.5f);
					break;
				}

				if (!key.m_bUseCurColor || nActiveKeyIndex == 0)
				{
					m_startColor = key.m_fadeColor;
				}
				else
				{
					SScreenFaderKey preKey;
					pTrack->GetKey(nActiveKeyIndex - 1, &preKey);
					CalculateIsolatedKeyColor(preKey, animContext.time, m_startColor);
				}

				if (key.m_fadeType == SScreenFaderKey::eFT_FadeIn)
				{
					if (!key.m_bUseCurColor || nActiveKeyIndex == 0)
					{
						m_startColor.w = 1.f;
					}

					key.m_fadeColor.w = 0.f;
				}
				else
				{
					if (!key.m_bUseCurColor || nActiveKeyIndex == 0)
					{
						m_startColor.w = 0.f;
					}

					key.m_fadeColor.w = 1.f;
				}

				pTrack->SetDrawColor(m_startColor + (key.m_fadeColor - m_startColor) * ratio);

				if (pTrack->GetDrawColor().w < 0.01f)
				{
					m_bActive = IsAnyTextureVisible();
				}
				else
				{
					m_bActive = true;
				}
			}
		}
		else
		{
			pTrack->SetTextureVisible(false);
			m_bActive = IsAnyTextureVisible();
		}
	}
}

void CAnimScreenFaderNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_ScreenFader);
}

void CAnimScreenFaderNode::OnReset()
{
	CAnimNode::OnReset();
	m_bActive = false;
}

void CAnimScreenFaderNode::Activate(bool bActivate)
{
	m_bActive = false;

	if (m_texPrecached == false)
	{
		PrecacheTexData();
	}
}

void CAnimScreenFaderNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);

	if (bLoading)
	{
		PrecacheTexData();
	}
}

unsigned int CAnimScreenFaderNode::GetParamCount() const
{
	return s_screenFaderNodeParams.size();
}

CAnimParamType CAnimScreenFaderNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < s_screenFaderNodeParams.size())
	{
		return s_screenFaderNodeParams[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CAnimScreenFaderNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < s_screenFaderNodeParams.size(); ++i)
	{
		if (s_screenFaderNodeParams[i].paramType == paramId)
		{
			info = s_screenFaderNodeParams[i];
			return true;
		}
	}

	return false;
}

void CAnimScreenFaderNode::Render()
{
	if (!m_bActive)
	{
		return;
	}

	if (gEnv->pRenderer)
	{
		size_t const paramCount = m_tracks.size();

		for (size_t paramIndex = 0; paramIndex < paramCount; ++paramIndex)
		{
			CScreenFaderTrack* pTrack = static_cast<CScreenFaderTrack*>(GetTrackForParameter(eAnimParamType_ScreenFader, paramIndex));

			if (!pTrack || (pTrack->GetFlags() & IAnimTrack::eAnimTrackFlags_Disabled) != 0)
			{
				continue;
			}

			int textureId = -1;

			if (pTrack->IsTextureVisible())
			{
				textureId = (pTrack->GetActiveTexture() != 0) ? pTrack->GetActiveTexture()->GetTextureID() : -1;
			}

			gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);
			gEnv->pRenderer->Draw2dImage(0, 0, m_screenWidth, m_screenHeight, textureId, 0.0f, 0.0f, 1.0f, 1.0f, 0.f,
			                             pTrack->GetDrawColor().x, pTrack->GetDrawColor().y, pTrack->GetDrawColor().z, pTrack->GetDrawColor().w, 0.f);
		}
	}
}

bool CAnimScreenFaderNode::IsAnyTextureVisible() const
{
	size_t const paramCount = m_tracks.size();

	for (size_t paramIndex = 0; paramIndex < paramCount; ++paramIndex)
	{
		CScreenFaderTrack* pTrack = static_cast<CScreenFaderTrack*>(GetTrackForParameter(eAnimParamType_ScreenFader, paramIndex));

		if (!pTrack)
		{
			continue;
		}

		if (pTrack->IsTextureVisible())
		{
			return true;
		}
	}

	return false;
}

void CAnimScreenFaderNode::PrecacheTexData()
{
	size_t const paramCount = m_tracks.size();

	for (size_t paramIndex = 0; paramIndex < paramCount; ++paramIndex)
	{
		IAnimTrack* pTrack = m_tracks[paramIndex];

		if (!pTrack)
		{
			continue;
		}

		switch (m_tracks[paramIndex]->GetParameterType().GetType())
		{
		case eAnimParamType_ScreenFader:
			{
				CScreenFaderTrack* pFaderTrack = static_cast<CScreenFaderTrack*>(pTrack);
				pFaderTrack->PreloadTextures();
			}
			break;
		}
	}

	m_texPrecached = true;
}
