// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EditorDialog.h"
#include "SandboxWindowing.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QPointer>
#include <QTimer>
#include <QKeyEvent>

QVariantMap CEditorDialog::s_config = QVariantMap();


CEditorDialog::CEditorDialog(const QString& moduleName, QWidget* parent /*= nullptr*/, bool saveSize)
	: QDialog(parent ? parent : QApplication::activeWindow(), Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint | Qt::FramelessWindowHint)
	, m_dialogNameId(moduleName)
	, m_layoutWrapped(false)
	, m_grid(0)
	, m_titleBar(0)
	, m_saveSize(saveSize)
	, m_resizable(true)
	, m_canClose(true)
{
	setSizeGripEnabled(false);

	if (!m_saveSize)
		return;

	AddPersonalizedSharedProperty(QStringLiteral("size"), [this]
	{
		auto size = this->size();
		QVariantMap map;
		map.insert(QStringLiteral("width"), size.width());
		map.insert(QStringLiteral("height"), size.height());
		return QVariant(map);
	}, [this](const QVariant& variant)
	{
		if (!variant.canConvert<QVariantMap>())
		{
		  return;
		}
		const auto map = variant.toMap();
		if (!map.contains(QStringLiteral("width")) || !map.contains(QStringLiteral("height")))
		{
		  return;
		}
		const QVariant& width = map["width"];
		const QVariant& height = map["height"];
		if (!width.canConvert<int>() || !height.canConvert<int>())
		{
		  return;
		}
		auto size = QSize(width.toInt(), height.toInt());
		QPointer<CEditorDialog> self(this);
		//Use a timer to ensure window is on right monitor in order to get correct DPI scaling
		QTimer::singleShot(0, [size, self] { 
			if (self)
			{
				QDesktopWidget* dw = qApp->desktop();

				for (int i = 0; i < dw->screenCount(); i++)
				{
					if (dw->screenGeometry(i).contains(QRect(self->pos(), QPoint(self->pos().x() + size.width(), self->pos().y() + size.height()))))
					{
						self->resize(size);
						break;
					}
				}
			} 
		});
	});

	AddPersonalizedSharedProperty(QStringLiteral("pos"), [this]
	{
		auto pos = this->pos();
		QVariantMap map;
		map.insert(QStringLiteral("x"), pos.x());
		map.insert(QStringLiteral("y"), pos.y());
		return QVariant(map);
	}, [this](const QVariant& variant)
	{
		if (!variant.canConvert<QVariantMap>())
		{
			return;
		}
		const auto map = variant.toMap();
		if (!map.contains(QStringLiteral("x")) || !map.contains(QStringLiteral("y")))
		{
			return;
		}
		const QVariant& x = map["x"];
		const QVariant& y = map["y"];
		if (!x.canConvert<int>() || !y.canConvert<int>())
		{
			return;
		}
		auto pos = QPoint(x.toInt(), y.toInt());
		QDesktopWidget* dw = qApp->desktop();

		for (int i = 0; i < dw->screenCount(); i++)
		{
			if (dw->screenGeometry(i).contains(QRect(pos, QPoint(pos.x() + size().width(), pos.y() + size().height()))))
			{
				move(pos);
				break;
			}
		}
	});
}

void CEditorDialog::AddPersonalizedSharedProperty(const QString& propName, std::function<QVariant()>& store, std::function<void(const QVariant&)>& restore)
{
	{
		auto pEditor = GetIEditor();

		if (pEditor)
		{
			CPersonalizationManager* pManager = pEditor->GetPersonalizationManager();

			auto variant = pManager->GetProperty(GetDialogName(), propName);
			if (variant.isValid())
			{
				restore(variant);
			}
		}
	}

	connect(this, &QDialog::finished, [this, propName, store]()
	{
		auto pEditor = GetIEditor();

		if (pEditor)
		{
			CPersonalizationManager* pManager = pEditor->GetPersonalizationManager();

			auto value = store();
			pManager->SetProperty(GetDialogName(), propName, value);
		}
	});
}

void CEditorDialog::AddPersonalizedProjectProperty(const QString& propName, std::function<QVariant()>& store, std::function<void(const QVariant&)>& restore)
{
	{
		auto pEditor = GetIEditor();

		if (pEditor)
		{
			CPersonalizationManager* pManager = pEditor->GetPersonalizationManager();

			auto variant = pManager->GetProjectProperty(GetDialogName(), propName);
			if (variant.isValid())
			{
				restore(variant);
			}
		}
	}

	connect(this, &QDialog::finished, [this, propName, store]()
	{
		auto pEditor = GetIEditor();

		if (pEditor)
		{
			CPersonalizationManager* pManager = pEditor->GetPersonalizationManager();

			auto value = store();
			pManager->SetProjectProperty(GetDialogName(), propName, value);
		}
	});
}

void CEditorDialog::showEvent(QShowEvent* event)
{
	if (!m_layoutWrapped)
	{
		// do not give this parent because in case
		// "this" already has a layout through a child
		// class it will generate a warning in QLayout.
		// setLayout() assigns the layout to the editor.
		m_grid = new QGridLayout();
		m_grid->setSpacing(0);
		m_grid->setContentsMargins(0, 0, 0, 0);
		m_titleBar = new QSandboxTitleBar(this, s_config);
		m_grid->addWidget(m_titleBar, 0, 0, 1, 3);
		m_grid->setRowMinimumHeight(0, 4);
		m_grid->setRowMinimumHeight(2, 4);
		m_grid->setColumnMinimumWidth(0, 4);
		m_grid->setColumnMinimumWidth(2, 4);
		QWidget* w = new QWidget(this);
		w->setLayout(layout());
		w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		m_grid->addWidget(w, 1, 1);
	#if defined(WIN32) || defined(WIN64)
		//Ensure titlebar has a HWND so it can receive native events
		HWND h = (HWND)m_titleBar->winId();
		SetWindowLong((HWND)winId(), GWL_STYLE, WS_OVERLAPPED | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	#endif
		setLayout(m_grid);
		m_layoutWrapped = true;
	}
	QDialog::showEvent(event);

	if(m_layoutWrapped)
	{
		// since Qt 5.6 there is a bug where the internal coordinates calculations are not correct
		// We have to force recalculations to fix this, which is most easily done by forcing a move.
		// This was a problem in the custom file dialogs which became unusable
		QPoint p = pos();
		move(p + QPoint(1, 0));
		qApp->processEvents();
		move(p);
	}
}

void CEditorDialog::changeEvent(QEvent* e)
{
	if (e->type() == QEvent::WindowStateChange && m_layoutWrapped)
	{
		if (windowState() == Qt::WindowMaximized)
		{
			m_grid->setRowMinimumHeight(0, 0);
			m_grid->setRowMinimumHeight(2, 0);
			m_grid->setColumnMinimumWidth(0, 0);
			m_grid->setColumnMinimumWidth(2, 0);
		}
		else
		{
			m_grid->setRowMinimumHeight(0, 4);
			m_grid->setRowMinimumHeight(2, 4);
			m_grid->setColumnMinimumWidth(0, 4);
			m_grid->setColumnMinimumWidth(2, 4);
		}
		m_titleBar->updateWindowStateButtons();
	}
	QWidget::changeEvent(e);
}

bool CEditorDialog::nativeEvent(const QByteArray& eventType, void* message, long* result)
{
#if (defined(WIN32) || defined(WIN64))
	MSG* msg = reinterpret_cast<MSG*>(message);
	QRect r = rect();
	static const int SIZE_MARGIN = 4;
	QPoint newPos = QCursor::pos();
	r.moveTo(pos());
	bool m_topEdge, m_leftEdge, m_rightEdge, m_bottomEdge;
	m_topEdge = qAbs(newPos.y() - r.top()) <= SIZE_MARGIN;
	m_leftEdge = qAbs(newPos.x() - r.left()) <= SIZE_MARGIN;
	m_rightEdge = qAbs(newPos.x() - r.right()) <= SIZE_MARGIN;
	m_bottomEdge = qAbs(newPos.y() - r.bottom()) <= SIZE_MARGIN;
	bool hor = m_leftEdge || m_rightEdge;
	bool ver = m_topEdge || m_bottomEdge;

	switch (msg->message)
	{
	case WM_SIZE:
		{
			RECT r;
			GetWindowRect((HWND)winId(), &r);
			OffsetRect(&r, -r.left, -r.top);
			HRGN rgn = CreateRectRgnIndirect(&r);
			SetWindowRgn((HWND)winId(), rgn, TRUE);
		}
		return false;
		break;
	case WM_NCACTIVATE:
		if (m_titleBar)
		{
			m_titleBar->setActive(msg->wParam);
		}
		*result = TRUE;
		return true;
		break;
	case WM_NCPAINT:
		*result = 0;
		return true;
		break;
	case WM_NCCALCSIZE:
		if (msg->wParam)
		{
			*result = 0;
			return true;
		}
		break;
	case WM_NCHITTEST:
		if (!m_resizable)
		{
			*result = HTCLIENT;
		}
		else if (hor && ver)
		{
			if (m_leftEdge == m_bottomEdge)
			{
				*result = m_leftEdge ? HTBOTTOMLEFT : HTTOPRIGHT;
			}
			else
			{
				*result = m_leftEdge ? HTTOPLEFT : HTBOTTOMRIGHT;
			}
		}
		else if (hor)
		{
			*result = m_leftEdge ? HTLEFT : HTRIGHT;
		}
		else if (ver)
		{
			*result = m_topEdge ? HTTOP : HTBOTTOM;
		}
		else
		{
			*result = HTCLIENT;
		}
		return true;
		break;
	case WM_GETMINMAXINFO:
		{
			PMINMAXINFO mmi = reinterpret_cast<PMINMAXINFO>(msg->lParam);
			const POINT ptZero = { 0, 0 };
			HMONITOR hmon = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
			HMONITOR hwndmon = MonitorFromWindow((HWND)winId(), MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi, mi2;
			mi.cbSize = sizeof(mi);
			mi2.cbSize = sizeof(mi2);
			GetMonitorInfo(hmon, &mi);
			GetMonitorInfo(hwndmon, &mi2);
			int miWidth = mi.rcWork.right - mi.rcWork.left;
			int miHeight = mi.rcWork.bottom - mi.rcWork.top;
			int mi2Width = mi2.rcWork.right - mi2.rcWork.left;
			int mi2Height = mi2.rcWork.bottom - mi2.rcWork.top;

			mmi->ptMaxPosition.x = mi2.rcWork.left - mi2.rcMonitor.left;
			mmi->ptMaxPosition.y = mi2.rcWork.top - mi2.rcMonitor.top;
			if (mi2Width > miWidth || mi2Height > miHeight)
			{
				mmi->ptMaxSize.x = mi.rcMonitor.right;
				mmi->ptMaxSize.y = mi.rcMonitor.bottom;
			}
			else
			{
				mmi->ptMaxSize.x = mi2Width;
				mmi->ptMaxSize.y = mi2Height;
			}
			setMaximumSize(mi2Width / devicePixelRatioF(), mi2Height / devicePixelRatioF()); //Ensure window will not overlap taskbar
			mmi->ptMaxTrackSize.x = min(this->maximumWidth() * devicePixelRatioF(), (qreal)GetSystemMetrics(SM_CXMAXTRACK));
			mmi->ptMaxTrackSize.y = min(this->maximumHeight() * devicePixelRatioF(), (qreal)GetSystemMetrics(SM_CYMAXTRACK));
			mmi->ptMinTrackSize.x = this->minimumWidth() * devicePixelRatioF();
			mmi->ptMinTrackSize.y = this->minimumHeight() * devicePixelRatioF();
			*result = 0;
			return true;
		}
		break;
	}
#endif
	return false;
}

void CEditorDialog::keyPressEvent(QKeyEvent* keyEvent)
{
	if (m_canClose || keyEvent->key() != Qt::Key_Escape)
		QDialog::keyPressEvent(keyEvent);
}

void CEditorDialog::SetResizable(bool resizable)
{
	if (resizable != m_resizable)
	{
		m_resizable = resizable;
		if (!m_resizable)
			setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	}
}

bool CEditorDialog::Execute()
{
	return exec() == QDialog::Accepted;
}

void CEditorDialog::Raise()
{
	if(window())
		window()->raise();
}

void CEditorDialog::Popup()
{
	show();
	Raise();
}

void CEditorDialog::Popup(const QPoint &position, const QSize& size)
{
	resize(size);
	Popup();
	if(window())
		window()->move(position);
}

void CEditorDialog::SetPosCascade()
{
	if (window())
	{
		static int i = 0;
		window()->move(QPoint(32, 32) * (i + 1));
		i = (i + 1) % 10;
	}
}

void CEditorDialog::SetHideOnClose()
{
	connect(this, &QDialog::finished, [this]()
	{
		hide();
	});
}

void CEditorDialog::SetDoNotClose()
{
	auto flags = windowFlags();
	setWindowFlags(flags & ~Qt::WindowCloseButtonHint);
	m_canClose = false;
}

void CEditorDialog::SetTitle(const QString& title)
{
	setWindowTitle(title);
}

