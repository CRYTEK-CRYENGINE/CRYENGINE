// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QObject>
#include <CrySandbox/CrySignal.h>

namespace ACE
{
struct IEditorImpl;

class CImplementationManager final : public QObject
{
	Q_OBJECT

public:

	CImplementationManager();
	virtual ~CImplementationManager() override;

	bool                LoadImplementation();
	void                Release();
	IEditorImpl*        GetImplementation();

	CCrySignal<void()> signalImplementationAboutToChange;
	CCrySignal<void()> signalImplementationChanged;

private:

	IEditorImpl* m_pEditorImpl;
	HMODULE      m_hMiddlewarePlugin;
};
} // namespace ACE
