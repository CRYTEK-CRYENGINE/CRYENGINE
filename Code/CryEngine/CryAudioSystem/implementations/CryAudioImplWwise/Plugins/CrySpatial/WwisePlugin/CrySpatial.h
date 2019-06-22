// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <afxwin.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
namespace Plugins
{
class CrySpatialApp final : public CWinApp
{
public:

	CrySpatialApp() = default;

	BOOL InitInstance() override;
	DECLARE_MESSAGE_MAP()
};
}// namespace Plugins
}// namespace Wwise
}// namespace Impl
}// namespace CryAudio