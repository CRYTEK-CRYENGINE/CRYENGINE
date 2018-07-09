// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "AutoLodSettings.h"

#include <CryString/CryString.h>
#include <CrySerialization/IArchive.h>
#include <CrySerialization/yasli/Archive.h>
#include <CrySerialization/yasli/STL.h>
#include <CrySerialization/yasli/Enum.h>
#include <CrySerialization/yasli/JSONIArchive.h>
#include <CrySerialization/yasli/JSONOArchive.h>

void CAutoLodSettings::sGlobalParams::Serialize(yasli::Archive& ar)
{
	ar(m_fViewreSolution, "viewresolution", "ViewResolution");
	ar(m_iViewsAround, "viewsaround", "ViewsAround");
	ar(m_iViewElevations, "viewelevations", "ViewElevations");
	ar(m_fSilhouetteWeight, "silhouetteweight", "SilhouetteWeight");
	ar(m_fVertexWelding, "vertexwelding", "VertexWelding");
	ar(m_bCheckTopology, "checktopology", "CheckTopology");
	ar(m_bObjectHasBase, "objecthasbase", "ObjectHasBase");
}

void CAutoLodSettings::sNodeParam::Serialize(yasli::Archive& ar)
{
	ar(m_bAutoGenerate, "autogenerate", "AutoGenerate");
	ar(m_iLodCount, "lodcount", "LodCount");
	ar(m_fPercent, "percent", "Percent");
}

bool CAutoLodSettings::sNodeParam::operator==(const CAutoLodSettings::sNodeParam& other) const
{
	if (!m_bAutoGenerate && !other.m_bAutoGenerate)
	{
		return true;
	}
	else
	{
		if (m_iLodCount == other.m_iLodCount && m_fPercent == other.m_fPercent)
		{
			return true;
		}
	}

	return false;
}

void CAutoLodSettings::sNode::Serialize(yasli::Archive& ar)
{
	ar(m_nodeName, "name", "Name");
	ar.doc(m_nodeName);

	ar(m_nodeParam, "nodeparam", "NodeParam");
}

// void CAutoLodSettings::sNodeParams::Serialize(yasli::Archive& ar)
// {
//  ar(nodeList, "nodeparams", "NodeParams");
// }

CAutoLodSettings::sGlobalParams& CAutoLodSettings::getGlobalParams()
{
	return m_globalParams;
}

std::vector<CAutoLodSettings::sNode>& CAutoLodSettings::getNodeList()
{
	return m_nodeParams;
}

void CAutoLodSettings::Serialize(yasli::Archive& ar)
{
	ar(m_globalParams, "globalparams", "GlobalParams");
	ar(m_nodeParams, "nodeparams", "NodeParams");
}

