// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TagComment.h"
#include "Objects/ObjectLoader.h"

REGISTER_CLASS_DESC(CTagCommentClassDesc);

//////////////////////////////////////////////////////////////////////////
// CBase implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CTagComment, CEntityObject)

//////////////////////////////////////////////////////////////////////////
CTagComment::CTagComment()
{
	SetColor(RGB(255, 160, 0));

	m_entityClass = "Comment";

	UseMaterialLayersMask(false);
}

//////////////////////////////////////////////////////////////////////////
void CTagComment::Serialize(CObjectArchive& ar)
{
	CEntityObject::Serialize(ar);

	// 141022 - for backward compatibility, check "Comment" attribute in the xml node
	// , retrieve all old CTagComment data, and write it on the properties.
	XmlNodeRef xmlNode = ar.node;
	if (ar.bLoading)
	{
		// Loading
		string commentText = "";
		xmlNode->getAttr("Comment", commentText);
		if (!commentText.IsEmpty())
		{
			// set script properties with old CTagComment values.
			CVarBlock* pProperties = GetProperties();
			if (pProperties)
			{
				IVariable* pVar = pProperties->FindVariable("Text");
				if (pVar)
					pVar->Set(commentText);

				float fSize = 0;
				bool bHidden = false;
				float fMaxDistance = 0;
				int nCharsPerLine = 0;
				bool bFixed = false;
				Vec3 diffuseColor = Vec3(0, 0, 0);

				pVar = pProperties->FindVariable("fSize");
				if (pVar && xmlNode->getAttr("Size", fSize))
					pVar->Set(fSize);

				pVar = pProperties->FindVariable("bHidden");
				if (pVar && xmlNode->getAttr("Hidden", bHidden))
					pVar->Set(bHidden);

				pVar = pProperties->FindVariable("fMaxDist");
				if (pVar && xmlNode->getAttr("MaxDistance", fMaxDistance))
					pVar->Set(fMaxDistance);

				pVar = pProperties->FindVariable("nCharsPerLine");
				if (pVar && xmlNode->getAttr("CharsPerLine", nCharsPerLine))
					pVar->Set(nCharsPerLine);

				pVar = pProperties->FindVariable("bFixed");
				if (pVar && xmlNode->getAttr("Fixed", bFixed))
					pVar->Set(bFixed);

				pVar = pProperties->FindVariable("clrDiffuse");
				if (pVar && xmlNode->getAttr("Color", diffuseColor))
					pVar->Set(diffuseColor);
			}
		}
	}
	else
	{
		// Saving.
		// Old "Comment" attribute will be gone once it's saved, and no special treatment is needed.
	}
}

