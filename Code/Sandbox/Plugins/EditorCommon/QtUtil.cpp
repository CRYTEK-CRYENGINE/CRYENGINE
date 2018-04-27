// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QtUtil.h"

// Qt
#include <QApplication>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QWidget>
#include <QLayout>
#include <QScrollArea>
#include <QProcess>
#include <QDir>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

// EditorCommon
#include "EditorStyleHelper.h"
#include "ProxyModels/MergingProxyModel.h"

namespace QtUtil
{

bool IsModifierKeyDown(int modifier)
{
	return QApplication::queryKeyboardModifiers() & modifier;
}

bool IsMouseButtonDown(int button)
{
	return QApplication::mouseButtons() & button;
}

EDITOR_COMMON_API QString CamelCaseToUIString(const char* camelCaseStr)
{
	QString out(camelCaseStr);

	// Replace underscore with space
	for (int i = 0; i < out.length(); ++i)
	{
		if (out[i] == '_')
			out[i] = ' ';
	}

	// Make first letters in words uppercase
	for (int i = 0; i < out.length(); ++i)
	{
		if ((i==0 || out[i - 1] == ' ') && out[i].isLower())
			out[i] = out[i].toUpper();
	}

	// Insert spaces between words
	for (int i = 1; i < out.length() - 1; ++i)
	{
		if ((out[i - 1].isLower() && out[i].isUpper()) || (out[i + 1].isLower() && out[i - 1].isUpper() && out[i].isUpper()))
			out.insert(i++, ' ');
	}

	return out;
}

EDITOR_COMMON_API QPoint GetMousePosition(QWidget* widget)
{
	return widget->mapFromGlobal(QCursor::pos());
}

EDITOR_COMMON_API QPoint GetMouseScreenPosition()
{
	return QCursor::pos();
}

EDITOR_COMMON_API bool IsParentOf(QObject* parent, QObject* child)
{
	CRY_ASSERT(child);

	QObject* o = child->parent();
	while (o)
	{
		if (o == parent)
		{
			return true;
		}
		else
		{
			o = o->parent();
		}
	}

	return false;
}

QString GetAppDataFolder()
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

void DrawStatePixmap(QPainter* painter, const QRect& iconRect, const QPixmap& pixmap, QStyle::State state)
{
	QPixmap normal = QPixmap(pixmap.size());
	normal.fill(Qt::transparent); // Enable alpha channel in the output
	QPainter p(&normal);
	p.drawPixmap(QPoint(0, 0), pixmap);
	p.setCompositionMode(QPainter::CompositionMode_Plus);
	if (state & QStyle::State_Selected)
		p.fillRect(normal.rect(), GetStyleHelper()->alternateSelectedIconTint()); // Mult with tree view custom tint color
	else if (state & QStyle::State_MouseOver)
		p.fillRect(normal.rect(), GetStyleHelper()->alternateHoverIconTint()); // Mult with tree view custom tint color
	p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
	p.drawPixmap(QPoint(0, 0), pixmap);
	painter->drawPixmap(iconRect.topLeft(), normal);
}

// Variant conversion helpers
QVariant ToQVariant(const QSize& size)
{
	QVariantMap map;
	map["width"] = size.width();
	map["height"] = size.height();

	return map;
}

QSize ToQSize(const QVariant& variant)
{
	if (variant.type() != QVariant::Map)
		return QSize();

	QVariantMap varMap = variant.toMap();
	return QSize(varMap["width"].toInt(), varMap["height"].toInt());
}

QVariant ToQVariant(const QByteArray& byteArray)
{
	return byteArray.toBase64();
}

QByteArray ToQByteArray(const QVariant& variant)
{
	return QByteArray::fromBase64(variant.toByteArray());
}

EDITOR_COMMON_API QScrollArea* MakeScrollable(QWidget* widget)
{
	auto scroll = new QScrollArea();
	scroll->setWidget(widget);
	scroll->setWidgetResizable(true);

	if (widget->layout())
	{
		widget->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
	}

	return scroll;
}

EDITOR_COMMON_API QScrollArea* MakeScrollable(QLayout* layout)
{
	auto widget = new QWidget();
	widget->setLayout(layout);

	return MakeScrollable(widget);
}

EDITOR_COMMON_API void OpenInExplorer(const char* path)
{
#ifdef CRY_PLATFORM_WINDOWS
	QFileInfo info(path);

	QStringList args;

	if (info.exists())
		args << "/select," << QDir::toNativeSeparators(path);
	else if(info.dir().exists())
		args << QDir::toNativeSeparators(info.dir().path());

	if (!args.isEmpty())
		QProcess::startDetached("explorer", args);
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Unable to open path: %s", path);
#else
	#error //Need platform specific implementation
#endif
}

EDITOR_COMMON_API void OpenFileForEdit(const char* filePath)
{
#ifdef CRY_PLATFORM_WINDOWS
	QFileInfo info(filePath);
	if(info.exists())
		QProcess::startDetached(filePath);
	else
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "File does not exist: %s", filePath);
#else
	#error //Need platform specific implementation
#endif
}

EDITOR_COMMON_API void RecursiveInstallEventFilter(QWidget* pListener, QWidget* pWatched)
{
	pWatched->installEventFilter(pListener);

	QList<QWidget*> childWidgets = pWatched->findChildren<QWidget*>();
	for (QWidget* pChild : childWidgets)
		RecursiveInstallEventFilter(pListener, pChild);
}

EDITOR_COMMON_API QAction* AddActionFromPath(const QString& menuPath, QMenu* parentMenu)
{
	QStringList splitPath = menuPath.split('/', QString::SkipEmptyParts);

	if (splitPath.isEmpty())
	{
		return nullptr;
	}

	QMenu* currentMenu = parentMenu;
	for (int i = 0; i < splitPath.size() - 1; i++)
	{
		bool bFound = false;

		QList<QMenu*> childMenus = currentMenu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);

		for (QMenu* menu : childMenus)
		{
			if (menu->title() == splitPath[i])
			{
				bFound = true;
				currentMenu = menu;
				break;
			}
		}

		if (!bFound)
		{
			currentMenu = currentMenu->addMenu(splitPath[i]);
		}
	}

	return currentMenu->addAction(splitPath.last());
}

EDITOR_COMMON_API int PixelScale(const QWidget* widget, int qtValue)
{
	return qtValue * widget->devicePixelRatioF();
}

EDITOR_COMMON_API float PixelScale(const QWidget* widget, float qtValue)
{
	return qtValue * widget->devicePixelRatioF();
}

EDITOR_COMMON_API qreal PixelScale(const QWidget* widget, qreal qtValue)
{
	return qtValue * widget->devicePixelRatioF();
}

EDITOR_COMMON_API QPoint PixelScale(const QWidget* widget, const QPoint& qtValue)
{
	return QPoint(PixelScale(widget, qtValue.x()), PixelScale(widget, qtValue.y()));
}

EDITOR_COMMON_API QPointF PixelScale(const QWidget* widget, const QPointF& qtValue)
{
	return QPointF(PixelScale(widget, qtValue.x()), PixelScale(widget, qtValue.y()));
}

EDITOR_COMMON_API int QtScale(const QWidget* widget, int pixelValue)
{
	return pixelValue / widget->devicePixelRatioF();
}

EDITOR_COMMON_API float QtScale(const QWidget* widget, float pixelValue)
{
	return pixelValue / widget->devicePixelRatioF();
}

EDITOR_COMMON_API qreal QtScale(const QWidget* widget, qreal pixelValue)
{
	return pixelValue / widget->devicePixelRatioF();
}

EDITOR_COMMON_API QPoint QtScale(const QWidget* widget, const QPoint& pixelValue)
{
	return QPoint(QtScale(widget, pixelValue.x()), QtScale(widget, pixelValue.y()));
}

EDITOR_COMMON_API QPointF QtScale(const QWidget* widget, const QPointF& pixelValue)
{
	return QPointF(QtScale(widget, pixelValue.x()), QtScale(widget, pixelValue.y()));
}

static int s_mouseCounter = 0;

EDITOR_COMMON_API void HideApplicationMouse()
{
	if (++s_mouseCounter == 1)
	{
		QApplication::setOverrideCursor(Qt::BlankCursor);
	}
}

EDITOR_COMMON_API void ShowApplicationMouse()
{
	if (--s_mouseCounter == 0)
	{
		QApplication::restoreOverrideCursor();
	}
}

namespace Private_QtUtil
{
	struct SProxyVariant
	{
		enum class Type
		{
			Abstract,
			Merging,
			Null
		};

		union
		{
			const QAbstractProxyModel* pAbstractProxyModel;
			const CMergingProxyModel* pMergingProxyModel;
		};
		const Type type;

		SProxyVariant()
			: type(Type::Null)
		{
		}

		explicit SProxyVariant(const QAbstractProxyModel* pProxy)
			: pAbstractProxyModel(pProxy)
			, type(Type::Abstract)
		{
		}

		explicit SProxyVariant(const CMergingProxyModel* pProxy)
			: pMergingProxyModel(pProxy)
			, type(Type::Merging)
		{
		}
	};

	SProxyVariant CastToProxy(const QAbstractItemModel* pModel)
	{
		const QAbstractProxyModel* const pAbstractProxyModel = qobject_cast<const QAbstractProxyModel*>(pModel);
		if (pAbstractProxyModel)
		{
			return SProxyVariant(pAbstractProxyModel);
		}
		const CMergingProxyModel* const pMergingProxyModel = qobject_cast<const CMergingProxyModel*>(pModel);
		if (pMergingProxyModel)
		{
			return SProxyVariant(pMergingProxyModel);
		}
		return SProxyVariant();
	}

	//! If there is a nested hierarchy of proxy models that contains both pProxyModel and pSourceModel,
	//! this function returns a path in that hierarchy from \p pProxyModel to \p pSourceModel.
	//! Otherwise, an empty path is returned.
	//! A path includes \p pProxyModel but excludes \p pSourceModel.
	//! The hierarchy of proxy models may contain sub-classes of QAbstractProxyModel and CMergingProxyModel.
	//! \p pSourceModel may be any sub-class of QAbstractItemModel.
	//! This function searches for the source model with depth-first traversal.
	//! Precondition: \p pProxyModel != pSourceModel.
	std::vector<SProxyVariant> FindSourceModel(const QAbstractItemModel* pProxyModel, const QAbstractItemModel* pSourceModel)
	{
		CRY_ASSERT(pProxyModel != pSourceModel);

		const SProxyVariant rootProxy = CastToProxy(pProxyModel);
		if (rootProxy.type == SProxyVariant::Type::Null)
		{
			return {};
		}

		// Invariant: both stacks are the same size.
		// It would be possible leave nextChildStack unchanged when a proxy of type Abstract is pushed or popped,
		// but the code is more terse when both stacks are kept in sync.
		std::vector<SProxyVariant> proxyStack;
		std::vector<int> nextChildStack;

		proxyStack.reserve(16); // Arbitrary initial size.
		nextChildStack.reserve(16);

		proxyStack.emplace_back(rootProxy);
		nextChildStack.push_back(0);

		while (!proxyStack.empty())
		{
			CRY_ASSERT(!nextChildStack.empty());

			SProxyVariant& proxy = proxyStack.back();

			if (proxy.type == SProxyVariant::Type::Abstract)
			{
				const QAbstractItemModel* const pChildModel = proxy.pAbstractProxyModel->sourceModel();

				if (pChildModel == pSourceModel)
				{
					return proxyStack;
				}

				const SProxyVariant child = CastToProxy(pChildModel);
				if (child.type != SProxyVariant::Type::Null)
				{
					proxyStack.push_back(child);
					nextChildStack.push_back(0);
				}
				else
				{
					proxyStack.pop_back();
					nextChildStack.pop_back();
				}
			}
			else
			{
				CRY_ASSERT(proxy.type == SProxyVariant::Type::Merging);

				int& nextItem = nextChildStack.back();
				if (nextItem >= proxy.pMergingProxyModel->GetSourceModelCount())
				{
					proxyStack.pop_back();
					nextChildStack.pop_back();
					continue;
				}

				const QAbstractItemModel* const pChildModel = proxy.pMergingProxyModel->GetSourceModel(nextItem++);

				if (pChildModel == pSourceModel)
				{
					return proxyStack;
				}

				const SProxyVariant child = CastToProxy(pChildModel);
				if (child.type != SProxyVariant::Type::Null)
				{
					proxyStack.push_back(child);
					nextChildStack.push_back(0);
				}
			}
		}

		return {};
	}
} // namespace Private_QtUtil

bool MapFromSourceIndirect(const QAbstractItemModel* pProxyModel, const QModelIndex& sourceIndexIn, QModelIndex& proxyIndexOut)
{
	using namespace Private_QtUtil;

	const QAbstractItemModel* const pSourceModel = sourceIndexIn.model();

	if (pProxyModel == pSourceModel)
	{
		proxyIndexOut = sourceIndexIn;
		return true;
	}

	std::vector<SProxyVariant> proxyPath = FindSourceModel(pProxyModel, pSourceModel);
	if (proxyPath.empty())
	{
		return false;
	}

	proxyIndexOut = sourceIndexIn;
	for (auto it = proxyPath.rbegin(); it != proxyPath.rend(); ++it)
	{
		const SProxyVariant& proxy = *it;
		if (proxy.type == SProxyVariant::Type::Abstract)
		{
			proxyIndexOut = proxy.pAbstractProxyModel->mapFromSource(proxyIndexOut);
		}
		else
		{
			CRY_ASSERT(proxy.type == SProxyVariant::Type::Merging);
			proxyIndexOut = proxy.pMergingProxyModel->MapFromSource(proxyIndexOut);
		}
	}
	return true;
}

EDITOR_COMMON_API bool MapFromSourceIndirect(const QAbstractItemView* pView, const QModelIndex& sourceIndexIn, QModelIndex& viewIndexOut)
{
	return MapFromSourceIndirect(pView->model(), sourceIndexIn, viewIndexOut);
}

}
