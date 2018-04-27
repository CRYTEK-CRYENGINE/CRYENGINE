// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "AutoRegister.h"
#include "ICommandManager.h"

#include <QWidget>

class CTrayArea;

typedef CAutoRegister<CTrayArea> CAutoRegisterTrayAreaHelper;

#define REGISTER_TRAY_AREA_WIDGET(Type, priority)                                      \
  namespace Internal                                                                   \
  {                                                                                    \
  void RegisterTrayWidget ## Type()                                                    \
  {                                                                                    \
    GetIEditor()->GetTrayArea()->RegisterTrayWidget<Type>(priority);                   \
  }                                                                                    \
  CAutoRegisterTrayAreaHelper g_AutoRegTrayHelper ## Type(RegisterTrayWidget ## Type); \
  }

class QHBoxLayout;

class EDITOR_COMMON_API CTrayArea : public QWidget
{
	class CTrayObject
	{
	public:
		CTrayObject(QWidget* pWidget = nullptr, int priority = -1)
			: m_pWidget(pWidget)
			, m_priority(priority)
		{
		}

		int      GetPriority() const { return m_priority; }
		QWidget* GetWidget() const   { return m_pWidget; }

	protected:
		int      m_priority;
		QWidget* m_pWidget;
	};

	Q_OBJECT
public:

	CTrayArea(QWidget* pParent = nullptr);
	~CTrayArea();

	// Tray area controls the lifetime of the widget
	template<typename T>
	void RegisterTrayWidget(int priority)
	{
		// No duplicates
		if (findChild<T*>("", Qt::FindDirectChildrenOnly))
			return;

		T* pTrayWidget = new T(this);
		CTrayObject* pTrayObject = new CTrayObject(pTrayWidget, priority);

		AddTrayObject(pTrayObject);
	}

	template<typename T>
	void UnRegisterTrayWidget()
	{
		T* pTrayWidget = findChild<T*>("", Qt::FindDirectChildrenOnly);
		if (pTrayWidget)
			pTrayWidget->deleteLater();
	}

	template<typename T>
	T* GetTrayWidget()
	{
		return findChild<T*>("", Qt::FindDirectChildrenOnly);
	}

signals:
	void MainFrameInitialized();

private:
	void AddTrayObject(CTrayObject* pTrayObject);
	void ReorderTrayObjects();

private:
	QHBoxLayout*          m_pMainLayout;
	QVector<CTrayObject*> m_trayObjects;
};

