// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Library.h"

#include "AudioControlsEditorPlugin.h"

namespace ACE
{
//////////////////////////////////////////////////////////////////////////
CLibrary::CLibrary(string const& name)
	: CAsset(name, EAssetType::Library)
	, m_pakStatus(EPakStatus::None)
{}

//////////////////////////////////////////////////////////////////////////
void CLibrary::SetModified(bool const isModified, bool const isForced /* = false */)
{
	if (!g_assetsManager.IsLoading() || isForced)
	{
		g_assetsManager.SetAssetModified(this, isModified);

		isModified ? (m_flags |= EAssetFlags::IsModified) : (m_flags &= ~EAssetFlags::IsModified);
	}
}

//////////////////////////////////////////////////////////////////////////
void CLibrary::SetPakStatus(EPakStatus const pakStatus, bool const exists)
{
	exists ? (m_pakStatus |= pakStatus) : (m_pakStatus &= ~pakStatus);
}
} // namespace ACE
