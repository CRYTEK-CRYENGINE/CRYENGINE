// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <QPropertyTree/ContextList.h>
#include <CrySchematyc2/Env/IEnvRegistry.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_Signalv2.h>

#include "DocGraphView.h"

class QBoxLayout;
class QParentWndWidget;
class QPropertyTree;

namespace Schematyc2
{
	enum class EDetailItemStatusFlags
	{
		None             = 0,
		ContainsWarnings = BIT(0),
		ContainsErrors   = BIT(1)
	};

	DECLARE_ENUM_CLASS_FLAGS(EDetailItemStatusFlags)

	struct IDetailItem
	{
		virtual ~IDetailItem() {}

		virtual SGUID GetGUID() const = 0;
		virtual Serialization::CContextList* GetContextList() = 0;
		virtual void SetStatusFlags(EDetailItemStatusFlags statusFlags) = 0;
		virtual void Serialize(Serialization::IArchive& archive) = 0;
	};

	DECLARE_SHARED_POINTERS(IDetailItem)

	class CEnvSettingsDetailItem : public IDetailItem
	{
	public:

		CEnvSettingsDetailItem(const IEnvSettingsPtr& pEnvSettings);

		// IDetailItem
		virtual SGUID GetGUID() const override;
		virtual Serialization::CContextList* GetContextList() override;
		virtual void SetStatusFlags(EDetailItemStatusFlags statusFlags) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~IDetailItem

	private:
		
		IEnvSettingsPtr	m_pEnvSettings;
	};

	DECLARE_SHARED_POINTERS(CEnvSettingsDetailItem)

	class CScriptElementDetailItem : public IDetailItem
	{
	public:

		CScriptElementDetailItem(TScriptFile* pScriptFile, IScriptElement* pScriptElement);

		// IDetailItem
		virtual SGUID GetGUID() const override;
		virtual Serialization::CContextList* GetContextList() override;
		virtual void SetStatusFlags(EDetailItemStatusFlags statusFlags) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~IDetailItem

	private:

		IScriptElement*                              m_pScriptElement;
		std::unique_ptr<Serialization::CContextList> m_pContextList;
	};

	DECLARE_SHARED_POINTERS(CScriptElementDetailItem)

	class CDocGraphViewNodeDetailItem  : public IDetailItem
	{
	public:

		CDocGraphViewNodeDetailItem(TScriptFile* pScriptFile, const CDocGraphViewNodePtr& pDocGraphViewNode);

		// IDetailItem
		virtual SGUID GetGUID() const override;
		virtual Serialization::CContextList* GetContextList() override;
		virtual void SetStatusFlags(EDetailItemStatusFlags statusFlags) override;
		virtual void Serialize(Serialization::IArchive& archive) override;
		// ~IDetailItem

	private:

		TScriptFile*                                 m_pScriptFile;
		CDocGraphViewNodePtr                         m_pDocGraphViewNode;
		std::unique_ptr<Serialization::CContextList> m_pContextList;
	};

	DECLARE_SHARED_POINTERS(CDocGraphViewNodeDetailItem)

	typedef TemplateUtils::CSignalv2<void (IDetailItem&)> DetailWidgetModificationSignal;

	struct SDetailWidgetSignals
	{
		DetailWidgetModificationSignal modification;
	};

	class CDetailWidget : public QWidget
	{
		Q_OBJECT

	public:

		CDetailWidget(QWidget* pParent);

		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);
		void AttachItem(const IDetailItemPtr& pItem);
		void Revert();
		void Detach();
		SDetailWidgetSignals& Signals();

	public slots:

		void OnPropertyChanged();

	private:

		void RefreshStatusFlags();

		QBoxLayout*    m_pLayout;
		QPropertyTree* m_pPropertyTree;
		IDetailItemPtr m_pItem;

		SDetailWidgetSignals m_signals;
	};

	class CDetailWnd : public CWnd
	{
		DECLARE_MESSAGE_MAP()

	public:

		CDetailWnd();

		~CDetailWnd();

		void Init();
		void InitLayout();
		void LoadSettings(const XmlNodeRef& xml);
		XmlNodeRef SaveSettings(const char* szName);
		void AttachItem(const IDetailItemPtr& pItem);
		void Revert();
		void Detach();
		void UpdateView();

		SDetailWidgetSignals& Signals();

	private:

		afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
		afx_msg void OnSize(UINT nType, int cx, int cy);

		QParentWndWidget* m_pParentWndWidget;
		CDetailWidget*    m_pDetailWidget;
		QBoxLayout*       m_pLayout;
	};
}
