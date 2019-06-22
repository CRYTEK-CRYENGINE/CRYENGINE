// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "CrySpatialPlugin.h"
#include <AK/Tools/Common/AkAssert.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
//////////////////////////////////////////////////////////////////////////
CrySpatialPlugin::CrySpatialPlugin()
	: m_pPSet(nullptr)
{
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialPlugin::Destroy()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CrySpatialPlugin::SetPluginPropertySet(AK::Wwise::IPluginPropertySet* in_pPSet)
{
	m_pPSet = in_pPSet;
}

//////////////////////////////////////////////////////////////////////////
bool CrySpatialPlugin::GetBankParameters(GUID const& in_guidPlatform, AK::Wwise::IWriteData* in_pDataWriter) const
{
	CComVariant varProp;
	m_pPSet->GetValue(in_guidPlatform, L"Dummy", varProp);
	in_pDataWriter->WriteReal32(varProp.fltVal);

	return true;
}
}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio