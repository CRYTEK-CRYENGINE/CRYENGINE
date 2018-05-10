// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ModelUtils.h"

#include "AudioControlsEditorPlugin.h"

namespace ACE
{
namespace ModelUtils
{
//////////////////////////////////////////////////////////////////////////
QStringList GetScopeNames()
{
	QStringList scopeNames;
	ScopeInfoList scopeInfoList;
	g_assetsManager.GetScopeInfoList(scopeInfoList);

	for (auto const& scopeInfo : scopeInfoList)
	{
		scopeNames.append(QString(scopeInfo.name));
	}

	scopeNames.sort(Qt::CaseInsensitive);

	return scopeNames;
}
} // namespace ModelUtils
} // namespace ACE

