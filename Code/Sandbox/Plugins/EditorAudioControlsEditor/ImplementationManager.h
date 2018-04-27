// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <CrySandbox/CrySignal.h>

#include <IImpl.h>

namespace ACE
{
extern Impl::IImpl* g_pIImpl;

class CImplementationManager final : public QObject
{
	Q_OBJECT

public:

	CImplementationManager();
	virtual ~CImplementationManager() override;

	bool LoadImplementation();
	void Release();

	CCrySignal<void()> SignalImplementationAboutToChange;
	CCrySignal<void()> SignalImplementationChanged;

private:

	HMODULE m_hMiddlewarePlugin;
};
} // namespace ACE

