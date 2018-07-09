// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryIcon.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QImageReader>
#include <QPalette>
#include <QPixmapCache>
#include <QString>
#include <QStringBuilder>
#include <QStyle>
#include <QStyleOption>
#include "QBitmap"

class QWindow;
#if QT_VERSION >= 0x050000
#include <QWindow>
#endif

#ifdef WIN32
#include <windows.h>
#endif

// This class is a modified version of QPixmapIconEngine from the Qt source code.

CryPixmapIconEngine::CryPixmapIconEngine(CryIconColorMap colorMap) : m_colormap(colorMap)
{
}

CryPixmapIconEngine::CryPixmapIconEngine(const CryPixmapIconEngine& other) : CryQtIconEngine(other), m_colormap(other.m_colormap), pixmaps(other.pixmaps)
{
}

CryPixmapIconEngine::~CryPixmapIconEngine()
{
}

static qreal qt_effective_device_pixel_ratio(QWindow* window = 0)
{
#if QT_VERSION >= 0x050000
	if (!qApp->testAttribute(Qt::AA_UseHighDpiPixmaps))
		return qreal(1.0);

	if (window)
		return window->devicePixelRatio();

	return qApp->devicePixelRatio(); // Don't know which window to target.
#else
	return qreal(1.0);
#endif
}

static inline int                area(const QSize& s) { return s.width() * s.height(); }

static CryPixmapIconEngineEntry* bestSizeMatch(const QSize& size, CryPixmapIconEngineEntry* pa, CryPixmapIconEngineEntry* pb)
{
	int s = area(size);
	if (pa->size == QSize() && pa->pixmap.isNull())
	{
		pa->pixmap = QPixmap(pa->fileName);
		pa->size = pa->pixmap.size();
	}
	int a = area(pa->size);
	if (pb->size == QSize() && pb->pixmap.isNull())
	{
		pb->pixmap = QPixmap(pb->fileName);
		pb->size = pb->pixmap.size();
	}
	int b = area(pb->size);
	int res = a;
	if (qMin(a, b) >= s)
		res = qMin(a, b);
	else
		res = qMax(a, b);
	if (res == a)
		return pa;
	return pb;
}

template<typename T>
struct HexString
{
	inline HexString(const T t)
		: val(t)
	{}

	inline void write(QChar*& dest) const
	{
		const ushort hexChars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
		const char* c = reinterpret_cast<const char*>(&val);
		for (uint i = 0; i < sizeof(T); ++i)
		{
			* dest++ = hexChars[*c & 0xf];
			* dest++ = hexChars[(*c & 0xf0) >> 4];
			++c;
		}
	}
	const T val;
};

template<typename T>
struct QConcatenable<HexString<T>>
{
	typedef HexString<T> type;
	enum { ExactSize = true };
	static int         size(const HexString<T>&)                      { return sizeof(T) * 2; }
	static inline void appendTo(const HexString<T>& str, QChar*& out) { str.write(out); }
	typedef QString ConvertTo;
};

void CryPixmapIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
	QSize pixmapSize = rect.size() * qt_effective_device_pixel_ratio(0);
	QPixmap px = pixmap(pixmapSize, mode, state);
	painter->drawPixmap(rect, px);
}

QPixmap applyQIconStyleHelper(QIcon::Mode mode, const QPixmap& base)
{
	QStyleOption opt(0);
	opt.palette = QGuiApplication::palette();
	return QApplication::style()->generatedIconPixmap(mode, base, &opt);
}

QPixmap CryPixmapIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
	QPixmap pm;
	CryPixmapIconEngineEntry* pe = bestMatch(size, mode, state, false);
	if (pe)
		pm = pe->pixmap;

	if (pm.isNull())
	{
		int idx = pixmaps.count();
		while (--idx >= 0)
		{
			if (pe == &pixmaps[idx])
			{
				pixmaps.remove(idx);
				break;
			}
		}
		if (pixmaps.isEmpty())
			return pm;
		else
			return pixmap(size, mode, state);
	}

	QSize actualSize = pm.size();
	if (!actualSize.isNull() && (actualSize.width() > size.width() || actualSize.height() > size.height()))
		actualSize.scale(size, Qt::KeepAspectRatio);

	QString key = QLatin1String("cry_")
	              % HexString<quint64>(pm.cacheKey())
	              % HexString<uint>(pe->mode)
	              % HexString<quint64>(QGuiApplication::palette().cacheKey())
	              % HexString<uint>(actualSize.width())
	              % HexString<uint>(actualSize.height());

	if (QPixmapCache::find(key % HexString<uint>(mode) % HexString<uint>(state), pm))
		return pm;

	if (pm.size() != actualSize)
		pm = pm.scaled(actualSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

	QPixmap normal = QPixmap(pm.size());
	normal.fill(Qt::transparent); // Enable alpha channel in the output
	QPainter p(&normal);
	p.drawPixmap(QPoint(0, 0), pm);


	//Go through pixels and change alpha channel to 0 if RGB values are equal --> colors from white over gray to black
	QImage imageColorOnly = pm.toImage();
	for (int i = 0; i < imageColorOnly.width(); i++)
	{
		for (int j = 0; j < imageColorOnly.height(); j++)
		{
			QRgb pixel = imageColorOnly.pixel(i, j);
			int red = qRed(pixel);
			int green = qGreen(pixel);
			int blue = qBlue(pixel);
			if (red == green && red == blue)
			{
				imageColorOnly.setPixelColor(i, j, QColor(red, green, blue, 0));
			}
		}
	}
	//This pixelmap now only contains the colored part of the image
	QPixmap pmColor = pmColor.fromImage(imageColorOnly);

	// Tint full image
	p.setCompositionMode(QPainter::CompositionMode_Multiply);
	QIcon::Mode brushMode = mode;
	if (state == QIcon::On && mode != QIcon::Disabled)
	{
		brushMode = QIcon::Selected;
	}
	p.fillRect(normal.rect(), getBrush(brushMode));

	//After tinting, overdraw with colored part of the image
	p.setCompositionMode(QPainter::CompositionMode_SourceOver);
	p.drawPixmap(QPoint(0, 0), pmColor);

	// Use original alpha channel to crop image
	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawPixmap(QPoint(0, 0), pm);

	QPixmapCache::insert(key % HexString<uint>(mode) % HexString<uint>(state), normal);
	return normal;
}

CryPixmapIconEngineEntry* CryPixmapIconEngine::bestMatch(const QSize& size, QIcon::Mode mode, QIcon::State state, bool sizeOnly)
{
	CryPixmapIconEngineEntry* pe = tryMatch(size, mode, state);
	while (!pe)
	{
		QIcon::State oppositeState = (state == QIcon::On) ? QIcon::Off : QIcon::On;
		if (mode == QIcon::Disabled || mode == QIcon::Selected)
		{
			QIcon::Mode oppositeMode = (mode == QIcon::Disabled) ? QIcon::Selected : QIcon::Disabled;
			if ((pe = tryMatch(size, QIcon::Normal, state)))
				break;
			if ((pe = tryMatch(size, QIcon::Active, state)))
				break;
			if ((pe = tryMatch(size, mode, oppositeState)))
				break;
			if ((pe = tryMatch(size, QIcon::Normal, oppositeState)))
				break;
			if ((pe = tryMatch(size, QIcon::Active, oppositeState)))
				break;
			if ((pe = tryMatch(size, oppositeMode, state)))
				break;
			if ((pe = tryMatch(size, oppositeMode, oppositeState)))
				break;
		}
		else
		{
			QIcon::Mode oppositeMode = (mode == QIcon::Normal) ? QIcon::Active : QIcon::Normal;
			if ((pe = tryMatch(size, oppositeMode, state)))
				break;
			if ((pe = tryMatch(size, mode, oppositeState)))
				break;
			if ((pe = tryMatch(size, oppositeMode, oppositeState)))
				break;
			if ((pe = tryMatch(size, QIcon::Disabled, state)))
				break;
			if ((pe = tryMatch(size, QIcon::Selected, state)))
				break;
			if ((pe = tryMatch(size, QIcon::Disabled, oppositeState)))
				break;
			if ((pe = tryMatch(size, QIcon::Selected, oppositeState)))
				break;
		}

		if (!pe)
			return pe;
	}

	if (sizeOnly ? (pe->size.isNull() || !pe->size.isValid()) : pe->pixmap.isNull())
	{
		pe->pixmap = QPixmap(pe->fileName);
		if (!pe->pixmap.isNull())
			pe->size = pe->pixmap.size();
	}

	return pe;
}

QSize CryPixmapIconEngine::actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
	QSize actualSize;
	if (CryPixmapIconEngineEntry* pe = bestMatch(size, mode, state, true))
		actualSize = pe->size;

	if (actualSize.isNull())
		return actualSize;

	if (!actualSize.isNull() && (actualSize.width() > size.width() || actualSize.height() > size.height()))
		actualSize.scale(size, Qt::KeepAspectRatio);
	return actualSize;
}

bool CryPixmapIconEngine::read(QDataStream& in)
{
	int num_entries;
	QPixmap pm;
	QString fileName;
	QSize sz;
	uint mode;
	uint state;

	in >> num_entries;
	for (int i = 0; i < num_entries; ++i)
	{
		if (in.atEnd())
		{
			pixmaps.clear();
			return false;
		}
		in >> pm;
		in >> fileName;
		in >> sz;
		in >> mode;
		in >> state;
		if (pm.isNull())
		{
			addFile(fileName, sz, QIcon::Mode(mode), QIcon::State(state));
		}
		else
		{
			CryPixmapIconEngineEntry pe(fileName, sz, QIcon::Mode(mode), QIcon::State(state));
			pe.pixmap = pm;
			pixmaps += pe;
		}
	}
	return true;
}

void CryPixmapIconEngine::addPixmap(const QPixmap& pixmap, QIcon::Mode mode, QIcon::State state)
{
	if (!pixmap.isNull())
	{
		CryPixmapIconEngineEntry* pe = tryMatch(pixmap.size(), mode, state);
		if (pe && pe->size == pixmap.size())
		{
			pe->pixmap = pixmap;
			pe->fileName.clear();
		}
		else
		{
			pixmaps += CryPixmapIconEngineEntry(pixmap, mode, state);
		}
	}
}

static inline int origIcoDepth(const QImage& image)
{
	const QString s = image.text(QStringLiteral("_q_icoOrigDepth"));
	return s.isEmpty() ? 32 : s.toInt();
}

static inline int findBySize(const QList<QImage>& images, const QSize& size)
{
	for (int i = 0; i < images.size(); ++i)
	{
		if (images.at(i).size() == size)
			return i;
	}
	return -1;
}

namespace {
class ImageReader
{
public:
	ImageReader(const QString& fileName) : m_reader(fileName), m_atEnd(false) {}

	QByteArray format() const { return m_reader.format(); }

	bool       read(QImage* image)
	{
		if (m_atEnd)
			return false;
		*image = m_reader.read();
		if (!image->size().isValid())
		{
			m_atEnd = true;
			return false;
		}
		m_atEnd = !m_reader.jumpToNextImage();
		return true;
	}

private:
	QImageReader m_reader;
	bool         m_atEnd;
};
} // namespace

void CryPixmapIconEngine::addFile(const QString& fileName, const QSize& size, QIcon::Mode mode, QIcon::State state)
{
	if (fileName.isEmpty())
		return;
	QString abs = fileName.startsWith(QLatin1Char(':')) ? fileName : QFileInfo(fileName).absoluteFilePath();
	bool ignoreSize = !size.isValid();
	ImageReader imageReaderTry(abs);
	QByteArray format = imageReaderTry.format();
	if (format.isEmpty()) // Device failed to open or unsupported format.
	{
		qWarning() << "Could not load icon at " << fileName;
		//Try the default Icon
		abs = QFileInfo("icons:common/general_icon_missing.ico").absoluteFilePath();
		ImageReader imageReaderNew(abs);
		format = imageReaderNew.format();
	}
	ImageReader imageReader(abs);
	QImage image;
	if (format != "ico")
	{
		if (ignoreSize)   // No size specified: Add all images.
		{
			while (imageReader.read(&image))
				pixmaps += CryPixmapIconEngineEntry(abs, image, mode, state);
		}
		else
		{
			// Try to match size. If that fails, add a placeholder with the filename and empty pixmap for the size.
			while (imageReader.read(&image) && image.size() != size)
			{
			}
			pixmaps += image.size() == size ?
			           CryPixmapIconEngineEntry(abs, image, mode, state) : CryPixmapIconEngineEntry(abs, size, mode, state);
		}
		return;
	}
	// Special case for reading Windows ".ico" files. Historically (QTBUG-39287),
	// these files may contain low-resolution images. As this information is lost,
	// ICOReader sets the original format as an image text key value. Read all matching
	// images into a list trying to find the highest quality per size.
	QList<QImage> icoImages;
	while (imageReader.read(&image))
	{
		if (ignoreSize || image.size() == size)
		{
			const int position = findBySize(icoImages, image.size());
			if (position >= 0)   // Higher quality available? -> replace.
			{
				if (origIcoDepth(image) > origIcoDepth(icoImages.at(position)))
					icoImages[position] = image;
			}
			else
			{
				icoImages.append(image);
			}
		}
	}
	foreach(const QImage &i, icoImages)
	pixmaps += CryPixmapIconEngineEntry(abs, i, mode, state);
	if (icoImages.isEmpty() && !ignoreSize) // Add placeholder with the filename and empty pixmap for the size.
		pixmaps += CryPixmapIconEngineEntry(abs, size, mode, state);
}

QString CryPixmapIconEngine::key() const
{
	return QLatin1String("CryPixmapIconEngine");
}

CryQtIconEngine* CryPixmapIconEngine::clone() const
{
	return new CryPixmapIconEngine(*this);
}

bool CryPixmapIconEngine::write(QDataStream& out) const
{
	int num_entries = pixmaps.size();
	out << num_entries;
	for (int i = 0; i < num_entries; ++i)
	{
		if (pixmaps.at(i).pixmap.isNull())
			out << QPixmap(pixmaps.at(i).fileName);
		else
			out << pixmaps.at(i).pixmap;
		out << pixmaps.at(i).fileName;
		out << pixmaps.at(i).size;
		out << (uint) pixmaps.at(i).mode;
		out << (uint) pixmaps.at(i).state;
	}
	return true;
}

void CryPixmapIconEngine::virtual_hook(int id, void* data)
{
	switch (id)
	{
	case CryQtIconEngine::AvailableSizesHook:
		{
		CryQtIconEngine::AvailableSizesArgument& arg =
			  *reinterpret_cast<CryQtIconEngine::AvailableSizesArgument*>(data);
			arg.sizes.clear();
			for (int i = 0; i < pixmaps.size(); ++i)
			{
				CryPixmapIconEngineEntry& pe = pixmaps[i];
				if (pe.size == QSize() && pe.pixmap.isNull())
				{
					pe.pixmap = QPixmap(pe.fileName);
					pe.size = pe.pixmap.size();
				}
				if (pe.mode == arg.mode && pe.state == arg.state && !pe.size.isEmpty())
					arg.sizes.push_back(pe.size);
			}
			break;
		}
	default:
		CryQtIconEngine::virtual_hook(id, data);
	}
}

CryPixmapIconEngineEntry* CryPixmapIconEngine::tryMatch(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
	CryPixmapIconEngineEntry* pe = 0;
	for (int i = 0; i < pixmaps.count(); ++i)
		if (pixmaps.at(i).mode == mode && pixmaps.at(i).state == state)
		{
			if (pe)
				pe = bestSizeMatch(size, &pixmaps[i], pe);
			else
				pe = &pixmaps[i];
		}
	return pe;
}

namespace DefaultTints
{
	QBrush Normal = QBrush(QColor(255, 255, 255));
	QBrush Disabled = QBrush(QColor(255, 255, 255));
	QBrush Active = QBrush(QColor(255, 255, 255));
	QBrush Selected = QBrush(QColor(255, 255, 255));
}

QBrush CryPixmapIconEngine::getBrush(QIcon::Mode mode)
{
	if (m_colormap.contains(mode))
	{
		return m_colormap[mode];
	}
	switch (mode)
	{
	case QIcon::Normal:
		return DefaultTints::Normal;
	case QIcon::Disabled:
		return DefaultTints::Disabled;
	case QIcon::Active:
		return DefaultTints::Active;
	case QIcon::Selected:
		return DefaultTints::Selected;
	default:
		return DefaultTints::Normal;
	}
}

CryIcon::CryIcon(CryIconColorMap colorMap /*= CryIconColorMap()*/) : QIcon(new CryPixmapIconEngine(colorMap))
{
}

CryIcon::CryIcon(const QString& filename, CryIconColorMap colorMap /*= CryIconColorMap()*/) : QIcon(new CryPixmapIconEngine(colorMap))
{
	addFile(filename);
}

CryIcon::CryIcon(const QPixmap& pixmap, CryIconColorMap colorMap /*= CryIconColorMap()*/) : QIcon(new CryPixmapIconEngine(colorMap))
{
	addPixmap(pixmap);
}

CryIcon::CryIcon(const QIcon& icon) : QIcon(icon)
{

}

void CryIcon::SetDefaultTint(QIcon::Mode mode, QBrush brush)
{
	switch (mode)
	{
	case QIcon::Normal:
		DefaultTints::Normal = brush;
		break;
	case QIcon::Disabled:
		DefaultTints::Disabled = brush;
		break;
	case QIcon::Active:
		DefaultTints::Active = brush;
		break;
	case QIcon::Selected:
		DefaultTints::Selected = brush;
		break;
	default:
		break;
	}
}

