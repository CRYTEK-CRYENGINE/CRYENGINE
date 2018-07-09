// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "StandaloneMaterialEditor.h"

#include "QMfcApp/qwinhost.h"

#include "Material/MaterialDialog.h"
#include "Material/MaterialManager.h"

#include "MaterialSender.h"
#include "IEditorImpl.h"

#include "Controls/QuestionDialog.h"
#include "QtUtil.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QAbstractNativeEventFilter>

#include <CrySystem/IProjectManager.h>

namespace
{

struct SMaterialEditorFilter : QAbstractNativeEventFilter
{
	virtual bool nativeEventFilter(const QByteArray& eventType, void* pMessage, long* pResult) override
	{
		if (eventType == "windows_generic_MSG")
		{
			const MSG* const pTheMessage = static_cast<const MSG*>(pMessage);
			if (pTheMessage->message == WM_MATEDITSEND)
			{
				GetIEditorImpl()->GetMaterialManager()->SyncMaterialEditor();

				if (pResult)
				{
					*pResult = 0;
				}

				return true;
			}
		}
		return false;
	}

	static SMaterialEditorFilter& GetInstance()
	{
		static SMaterialEditorFilter theInstance;
		return theInstance;
	}
};

} // namespace

StandaloneMaterialEditor::StandaloneMaterialEditor()
	: CEditorDialog("SandaloneMaterialEditor")
	, m_materialWnd(nullptr)
{
	auto winHost = new QWinHost();

	auto pClassDesc = GetIEditorImpl()->GetClassFactory()->FindClass(MATERIAL_EDITOR_NAME);
	if(pClassDesc)
	{
		CRuntimeClass* pRuntimeClass = pClassDesc->GetRuntimeClass();
		if (pRuntimeClass)
		{
			assert(pRuntimeClass->IsDerivedFrom(RUNTIME_CLASS(CWnd)) || pRuntimeClass == RUNTIME_CLASS(CWnd));
			CRY_ASSERT(AfxGetMainWnd());
			m_materialWnd = (CWnd*)pRuntimeClass->CreateObject();
			assert(m_materialWnd);
			winHost->setWindow(m_materialWnd->GetSafeHwnd());
		}
	}

	setAttribute(Qt::WA_DeleteOnClose);

	if (!m_materialWnd)
	{
		CQuestionDialog::SCritical("Error", "Error initializing the Standalone Material Editor");
		close();
	}
	else
	{
		auto pLayout = new QVBoxLayout(this);
		pLayout->setContentsMargins(5, 5, 5, 5);
		pLayout->addWidget(new QLabel(tr("Material editing mode. Restart Sandbox for full functionality.")), 0);
		pLayout->addWidget(winHost, 1);

		setMinimumSize(QSize(600, 400));
	}

	qApp->installNativeEventFilter(&SMaterialEditorFilter::GetInstance());

	QTimer::singleShot(16, this, &StandaloneMaterialEditor::OnIdleCallback);

	string sWindowTitle;
	sWindowTitle.Format("CRYENGINE - Game %s", gEnv->pSystem->GetIProjectManager()->GetCurrentProjectName());

	QString windowTitle(sWindowTitle);
	setWindowTitle(windowTitle);
}

StandaloneMaterialEditor::~StandaloneMaterialEditor()
{
	if(m_materialWnd)
		m_materialWnd->DestroyWindow();
}

void StandaloneMaterialEditor::Execute()
{
	if (m_materialWnd)
	{
		show();
		window()->raise();
	}
		
}

void StandaloneMaterialEditor::closeEvent(QCloseEvent * event)
{
	//when done, quit application
	GetIEditorImpl()->GetCommandManager()->Execute("general.exit");
}

void StandaloneMaterialEditor::OnIdleCallback()
{
	GetIEditorImpl()->Notify(eNotify_OnIdleUpdate);
	QTimer::singleShot(16, this, &StandaloneMaterialEditor::OnIdleCallback);
}

