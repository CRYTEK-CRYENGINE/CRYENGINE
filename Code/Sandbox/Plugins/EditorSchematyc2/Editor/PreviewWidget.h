// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <QObject.h>
#include <QViewportSettings.h>
#include <QWidget.h>
#include <CrySchematyc2/IFoundation.h>
#include <CrySchematyc2/IObject.h>

struct SRenderContext;
class  QBoxLayout;
class  QParentWndWidget;
class  QPropertyTree;
class  QPushButton;
class  QSplitter;
class  QViewport;

namespace Schematyc2
{
	class CPreviewWidget : public QWidget, public IEntityEventListener
	{
		Q_OBJECT

	public:

		CPreviewWidget(QWidget* pParent);

		~CPreviewWidget();

		// IEntityEventListener
		virtual void OnEntityEvent(IEntity* pEntity, const SEntityEvent& event) override;
		// ~IEntityEventListener

		void InitLayout();
		void Update();
		void SetClass(const IScriptClass* pClass);
		void Reset();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);
		void Serialize(Serialization::IArchive& archive);

	protected slots:

		void OnRender(const SRenderContext& context);
		void OnShowHideSettingsButtonClicked();

	private:

		void ResetCamera(const Vec3& orbitTarget, float orbitRadius) const;

		static const Vec3  DefaultOrbitTarget;
		static const float DefaultOrbitRadius;

		QBoxLayout*    m_pMainLayout;
		QSplitter*     m_pSplitter;
		QPropertyTree* m_pSettings;
		QViewport*     m_pViewport;
		QBoxLayout*    m_pControlLayout;
		QPushButton*   m_pShowHideSettingsButton;

		SViewportSettings              m_viewportSettings;
		const IScriptClass*            m_pClass;
		IFoundationPreviewExtensionPtr m_pFoundationPreviewExtension;
		IObject*                       m_pPreviewObject;
		IObjectPreviewPropertiesPtr    m_pPreviewProperties;
	};

	class CPreviewWnd : public CWnd
	{
		DECLARE_MESSAGE_MAP()

	public:

		CPreviewWnd();

		~CPreviewWnd();

		void Init();
		void InitLayout();
		void Update();
		void SetClass(const IScriptClass* pClass);
		void Reset();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);

	private:

		afx_msg void OnSize(UINT nType, int cx, int cy);

		QParentWndWidget*	m_pParentWndWidget;
		CPreviewWidget*		m_pPreviewWidget;
		QBoxLayout*				m_pLayout;
	};
}
