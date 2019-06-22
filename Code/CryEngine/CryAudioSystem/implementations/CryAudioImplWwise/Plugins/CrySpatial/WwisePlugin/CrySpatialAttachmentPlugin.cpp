// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CrySpatialAttachmentPlugin.h"
#include "CrySpatialFXFactory.h"

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
//////////////////////////////////////////////////////////////////////////
CrySpatialAttachmentPlugin::CrySpatialAttachmentPlugin()
	: m_pPSet(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialAttachmentPlugin::Destroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialAttachmentPlugin::SetPluginPropertySet(AK::Wwise::IPluginPropertySet* in_pPSet)
{
	m_pPSet = in_pPSet;
}

//////////////////////////////////////////////////////////////////////////
bool CrySpatialAttachmentPlugin::GetBankParameters(GUID const& in_guidPlatform, AK::Wwise::IWriteData* in_pDataWriter) const
{
	CComVariant varProp;
	m_pPSet->GetValue(in_guidPlatform, L"Cutoff", varProp);
	in_pDataWriter->WriteReal32(varProp.fltVal);

	return true;
}
}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio