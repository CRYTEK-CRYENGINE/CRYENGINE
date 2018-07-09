// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "DragDrop.h"

#include "QTrackingTooltip.h"

#include <QDrag>
#include <QFontMetrics>
#include <QTimer>
#include <QUrl>
#include <qevent.h>
#include <unordered_map>

//Notes for future usage:
//CDrag object can be retrived from a drag event
// QDrag* const pDrag = pDragEvent->source()->findChild<QDrag*>();

namespace Private_DragDrop
{

class CDragTooltip
{
public:
	enum class EDisplayMode
	{
		Clear = 0,
		Text,
		Pixmap
	};

private:
	struct SScope
	{
		EDisplayMode m_displayMode;
		QString m_text;
		QPixmap m_pixmap;

		SScope()
			: m_displayMode(EDisplayMode::Clear)
		{
		}
	};

public:
	static CDragTooltip& GetInstance()
	{
		static CDragTooltip theInstance;
		return theInstance;
	}

	void StartDrag()
	{
		m_scopes.clear();
		m_bDragging = true;
		m_timer.start(0);

		m_defaultScope = SScope();
	}

	void StopDrag()
	{
		m_bDragging = false;
		m_timer.stop();
		Update();
	}

	void SetText(QWidget* pWidget, const QString& text)
	{
		SScope& scope = GetScope(pWidget);
		scope.m_displayMode = EDisplayMode::Text;
		scope.m_text = text;
	}
	
	void SetPixmap(QWidget* pWidget, const QPixmap& pixmap)
	{
		SScope& scope = GetScope(pWidget);
		scope.m_displayMode = EDisplayMode::Pixmap;
		scope.m_pixmap = pixmap;
	}

	void Clear(QWidget* pWidget)
	{
		SScope& scope = GetScope(pWidget);
		scope.m_displayMode = EDisplayMode::Clear;
	}

	void SetDefaultPixmap(const QPixmap& pixmap)
	{
		m_defaultScope.m_displayMode = EDisplayMode::Pixmap;
		m_defaultScope.m_pixmap = pixmap;
	}

	void SetCursorOffset(const QPoint& point)
	{
		m_trackingTooltip->SetPosCursorOffset(point);
	}

	void Update()
	{
		if (!m_bDragging)
		{
			/*
			QTrackingTooltip::HideTooltip(&m_trackingTooltip);
			return;
			*/
		}

		QWidget* const pSubject = qApp->widgetAt(QCursor::pos());
		const SScope& scope = pSubject ? GetScope(pSubject) : m_defaultScope;

		if (scope.m_displayMode == EDisplayMode::Clear)
		{
			QTrackingTooltip::HideTooltip(m_trackingTooltip.data());
		}
		else
		{
			// Pixmap and text are mutually exclusive for a label.
			if (scope.m_displayMode == EDisplayMode::Text)
			{
				m_trackingTooltip->SetText(scope.m_text);
			}
			else if (scope.m_displayMode == EDisplayMode::Pixmap)
			{
				m_trackingTooltip->SetPixmap(scope.m_pixmap);
			}

			QTrackingTooltip::ShowTrackingTooltip(m_trackingTooltip);
		}
	}

private:
	CDragTooltip()
		: m_trackingTooltip(new QTrackingTooltip(), &QObject::deleteLater)
		, m_bDragging(false)
	{
		QObject::connect(&m_timer, &QTimer::timeout, [this]() { Update(); });
	}

	SScope& GetScope(QObject* pSubject)
	{
		if (m_scopes.find(pSubject) == m_scopes.end())
		{
			m_scopes[pSubject].reset(new SScope(m_defaultScope));
		}
		return *m_scopes[pSubject];
	}

private:
	std::unordered_map<const QObject*, std::unique_ptr<SScope>> m_scopes;
	SScope m_defaultScope;
	QSharedPointer<QTrackingTooltip> m_trackingTooltip;
	QTimer m_timer;
	bool m_bDragging;
};

} // namespace PrivateDragDrop

QString CDragDropData::GetMimeFormatForType(const char* type)
{
	QString typeStr(type);
	typeStr = typeStr.simplified();
	typeStr.replace(' ', '_');

	return QString("application/crytek;type=\"%1\"").arg(type);
}

const CDragDropData* CDragDropData::FromDragDropEvent(const QDropEvent* event)
{
	return FromMimeDataSafe(event->mimeData());
}

const CDragDropData* CDragDropData::FromMimeData(const QMimeData* mimeData)
{
	return static_cast<const CDragDropData*>(mimeData);
}

const CDragDropData* CDragDropData::FromMimeDataSafe(const QMimeData* mimeData)
{
	const CDragDropData* result = qobject_cast<const CDragDropData*>(mimeData);
	CRY_ASSERT_MESSAGE(result, "The QMimeData is not of type CDragDropData");
	return result;
}

QByteArray CDragDropData::GetCustomData(const char* type) const
{
	return QMimeData::data(GetMimeFormatForType(type));
}

bool CDragDropData::HasFilePaths() const
{
	// List of accepted MIME formats (in this order).
	return hasUrls();
	//These formats are not currently supported but are part of the mime types you get when drag&dropping from Windows explorer
/*
		|| hasFormat(QStringLiteral("application/x-qt-windows-mime;value=\"FileName\""))
		|| hasFormat(QStringLiteral("application/x-qt-windows-mime;value=\"FileNameW\""));
*/
}

QStringList CDragDropData::GetFilePaths() const
{
	//TODO : test drag&drop of several files from browser
	QStringList filePaths;

	// List of accepted MIME formats (in this order).
	if (hasUrls())
	{
		QList<QUrl> urls = QMimeData::urls();
		QList<QUrl>::const_iterator it = urls.constBegin();

		for (; it != urls.constEnd(); ++it) 
			filePaths.append((*it).toLocalFile());
	}

	return filePaths;
}

void CDragDropData::SetCustomData(const char* type, const QByteArray& data)
{
	QMimeData::setData(GetMimeFormatForType(type), data);
}

bool CDragDropData::HasCustomData(const char* type) const
{
	return QMimeData::hasFormat(GetMimeFormatForType(type));
}

void CDragDropData::ShowDragText(QWidget* pWidget, const QString& what)
{
	using namespace Private_DragDrop;
	CDragTooltip::GetInstance().SetText(pWidget, what);
}

void CDragDropData::ShowDragPixmap(QWidget* pWidget, const QPixmap& what, const QPoint& pixmapCursorOffset /*= QPoint()*/)
{
	using namespace Private_DragDrop;
	CDragTooltip::GetInstance().SetPixmap(pWidget, what);

	if(!pixmapCursorOffset.isNull())
		CDragTooltip::GetInstance().SetCursorOffset(pixmapCursorOffset);
}

void CDragDropData::ClearDragTooltip(QWidget* pWidget)
{
	using namespace Private_DragDrop;
	CDragTooltip::GetInstance().Clear(pWidget);
}

void CDragDropData::StartDrag(QObject* pDragSource, Qt::DropActions supportedActions, QMimeData* pMimeData, const QPixmap* pPixmap, const QPoint& pixmapCursorOffset /*= QPoint()*/)
{
	using namespace Private_DragDrop;
	QDrag* const pDrag = new QDrag(pDragSource);
	pDrag->setMimeData(pMimeData);
	CDragTooltip& tooltip = CDragTooltip::GetInstance();
	tooltip.StartDrag();
	CDragDropData* const pDragDropData = (CDragDropData*)pMimeData;
	if (pDragDropData && pPixmap)
	{
		tooltip.SetDefaultPixmap(*pPixmap);
		if (!pixmapCursorOffset.isNull())
			tooltip.SetCursorOffset(pixmapCursorOffset);
	}
	pDrag->exec(supportedActions);
	tooltip.StopDrag();
}

