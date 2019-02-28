// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CommentNode.h"
#include "AnimSplineTrack.h"

namespace
{
bool s_nodeParamsInit = false;
std::vector<CAnimNode::SParamInfo> s_nodeParameters;

void AddSupportedParameters(const char* sName, int paramId, EAnimValue valueType)
{
	CAnimNode::SParamInfo param;
	param.name = sName;
	param.paramType = paramId;
	param.valueType = valueType;
	s_nodeParameters.push_back(param);
}
};

CCommentNode::CCommentNode(const int id) : CAnimNode(id)
{
	CCommentNode::Initialize();
}

void CCommentNode::Initialize()
{
	if (!s_nodeParamsInit)
	{
		s_nodeParamsInit = true;
		s_nodeParameters.reserve(3);
		AddSupportedParameters("Text", eAnimParamType_CommentText, eAnimValue_Unknown);
		AddSupportedParameters("Unit Pos X", eAnimParamType_PositionX, eAnimValue_Float);
		AddSupportedParameters("Unit Pos Y", eAnimParamType_PositionY, eAnimValue_Float);
	}
}

void CCommentNode::CreateDefaultTracks()
{
	CreateTrack(eAnimParamType_CommentText);

	CAnimSplineTrack* pTrack = 0;

	pTrack = (CAnimSplineTrack*)CreateTrack(eAnimParamType_PositionX);
	pTrack->SetDefaultValue(TMovieSystemValue(50.0f));

	pTrack = (CAnimSplineTrack*)CreateTrack(eAnimParamType_PositionY);
	pTrack->SetDefaultValue(TMovieSystemValue(50.0f));
}

void CCommentNode::Activate(bool bActivate)
{
	CAnimNode::Activate(bActivate);
}

void CCommentNode::Serialize(XmlNodeRef& xmlNode, bool bLoading, bool bLoadEmptyTracks)
{
	CAnimNode::Serialize(xmlNode, bLoading, bLoadEmptyTracks);
}

unsigned int CCommentNode::GetParamCount() const
{
	return s_nodeParameters.size();
}

CAnimParamType CCommentNode::GetParamType(unsigned int nIndex) const
{
	if (nIndex < s_nodeParameters.size())
	{
		return s_nodeParameters[nIndex].paramType;
	}

	return eAnimParamType_Invalid;
}

bool CCommentNode::GetParamInfoFromType(const CAnimParamType& paramId, SParamInfo& info) const
{
	for (size_t i = 0; i < s_nodeParameters.size(); ++i)
	{
		if (s_nodeParameters[i].paramType == paramId)
		{
			info = s_nodeParameters[i];
			return true;
		}
	}

	return false;
}
