// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QMap>
#include <QIcon>
#include <QString>
#include <QPainter>

#if QT_VERSION < 0x050000
#include <QIconEngineV2>
typedef QIconEngineV2 CryQtIconEngine;
#else
#include <QIconEngine>
typedef QIconEngine CryQtIconEngine;
#endif

#include "CryQtAPI.h"
#include "CryQtCompatibility.h"

struct CryPixmapIconEngineEntry
{
	CryPixmapIconEngineEntry() : mode(QIcon::Normal), state(QIcon::Off){}
	CryPixmapIconEngineEntry(const QPixmap& pm, QIcon::Mode m = QIcon::Normal, QIcon::State s = QIcon::Off)
		: pixmap(pm), size(pm.size()), mode(m), state(s){}
	CryPixmapIconEngineEntry(const QString& file, const QSize& sz = QSize(), QIcon::Mode m = QIcon::Normal, QIcon::State s = QIcon::Off)
		: fileName(file), size(sz), mode(m), state(s){}
	CryPixmapIconEngineEntry(const QString& file, const QImage& image, QIcon::Mode m = QIcon::Normal, QIcon::State s = QIcon::Off);
	QPixmap      pixmap;
	QString      fileName;
	QSize        size;
	QIcon::Mode  mode;
	QIcon::State state;
	bool         isNull() const { return (fileName.isEmpty() && pixmap.isNull()); }
};

inline CryPixmapIconEngineEntry::CryPixmapIconEngineEntry(const QString& file, const QImage& image, QIcon::Mode m, QIcon::State s)
	: fileName(file), size(image.size()), mode(m), state(s)
{
	pixmap.convertFromImage(image);
#if QT_VERSION >= 0x0500000
	// Reset the devicePixelRatio. The pixmap may be loaded from a @2x file,
	// but be used as a 1x pixmap by QIcon.
	pixmap.setDevicePixelRatio(1.0);
#endif
}

typedef QMap<QIcon::Mode, QBrush> CryIconColorMap;

class CRYQT_API CryPixmapIconEngine : public CryQtIconEngine
{
public:
	CryPixmapIconEngine(CryIconColorMap colorMap = CryIconColorMap());
	CryPixmapIconEngine(const CryPixmapIconEngine&);
	~CryPixmapIconEngine();
	void                      paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;
	QPixmap                   pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;
	CryPixmapIconEngineEntry* bestMatch(const QSize& size, QIcon::Mode mode, QIcon::State state, bool sizeOnly);
	QSize                     actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;
	void                      addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;
	void                      addFile(const QString& fileName, const QSize& size, QIcon::Mode mode, QIcon::State state) Q_DECL_OVERRIDE;

	QString                   key() const Q_DECL_OVERRIDE;
	CryQtIconEngine* clone() const Q_DECL_OVERRIDE;
	bool                      read(QDataStream& in) Q_DECL_OVERRIDE;
	bool                      write(QDataStream& out) const Q_DECL_OVERRIDE;
	void                      virtual_hook(int id, void* data) Q_DECL_OVERRIDE;

private:
	CryPixmapIconEngineEntry* tryMatch(const QSize& size, QIcon::Mode mode, QIcon::State state);
	QBrush                    getBrush(QIcon::Mode mode);
	QVector<CryPixmapIconEngineEntry> pixmaps;
	CryIconColorMap                   m_colormap;
	friend class CryIcon;
};

class CRYQT_API CryIcon : public QIcon
{
public:
	explicit CryIcon(CryIconColorMap colorMap = CryIconColorMap());
	CryIcon(const QIcon& icon);
	explicit CryIcon(const QString& filename, CryIconColorMap colorMap = CryIconColorMap());
	explicit CryIcon(const QPixmap& pixmap, CryIconColorMap colorMap = CryIconColorMap());
	static void SetDefaultTint(QIcon::Mode mode, QBrush brush);
};

