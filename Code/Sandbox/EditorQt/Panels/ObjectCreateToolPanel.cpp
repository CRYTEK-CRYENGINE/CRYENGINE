// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "ObjectCreateToolPanel.h"

#include "QtUtil.h"

#include "ObjectCreateTreeWidget.h"
#include "ObjectBrowser.h"
#include "ObjectCreateTool.h"
#include "CryEditDoc.h"
#include "CryIcon.h"
#include "Controls/QMenuComboBox.h"

#include <QStackedLayout>
#include <QVboxLayout>
#include <QMouseEvent>
#include <QString>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QToolButton>
#include "EditorFramework/PersonalizationManager.h"

REGISTER_VIEWPANE_FACTORY_AND_MENU(CObjectCreateToolPanel, "Create Object", "Level Editor", false, "Level Editor");

#define default_animation_time     150
#define default_animation_distance 10000.0f

class ObjectButtonEnumerator : public IObjectEnumerator
{
public:
	virtual void AddEntry(const char* path, const char* id, const char* sortStr = "") override
	{
		m_entries[path] = id;
	}
	virtual void RemoveEntry(const char* path, const char* id) override
	{
		m_entries.erase(path);
	}
	virtual void ChangeEntry(const char* path, const char* id, const char* sortStr = "") override
	{
		m_entries[path] = id;
	}

	std::map<string, string>* GetEntries() { return &m_entries; }

private:
	std::map<string, string> m_entries;
};

//////////////////////////////////////////////////////////////////////////

CObjectCreateToolPanel::CObjectCreateToolPanel(QWidget* pParent /*= nullptr*/)
	: CDockableEditor(pParent)
	, m_stacked(new QStackedLayout())
{
	m_stacked->setStackingMode(QStackedLayout::StackAll);

	//Create top buttons
	m_pTypeButtonPanel = new CCreateObjectButtons(this);

	std::vector<string> categoryNames;
	GetIEditorImpl()->GetObjectManager()->GetClassCategories(categoryNames);

	for (int i = 0; i < categoryNames.size(); i++)
	{
		m_pTypeButtonPanel->AddButton(categoryNames[i], [=]() { OnObjectTypeSelected(categoryNames[i]); });
	}

	m_typeToStackIndex.push_back("typeButtonPanel");
	m_stacked->addWidget(m_pTypeButtonPanel);
	m_stacked->setAlignment(m_pTypeButtonPanel, Qt::AlignTop);

	SetContent(m_stacked);
}

CObjectCreateToolPanel::~CObjectCreateToolPanel()
{
}

void CObjectCreateToolPanel::mousePressEvent(QMouseEvent* pMouseEvent)
{
	//TODO : this should be done with a general.back type of command, which removes the need for recursive event filter
	if (pMouseEvent->button() == Qt::MouseButton::BackButton)
		OnBackButtonPressed();
}

bool CObjectCreateToolPanel::eventFilter(QObject* pWatched, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::MouseButtonPress)
		mousePressEvent(static_cast<QMouseEvent*>(pEvent));

	return false;
}

void CObjectCreateToolPanel::OnBackButtonPressed()
{
	// Go back to the main menu. Make sure the main menu is not the current widget
	if (m_stacked->widget(0) == m_stacked->currentWidget())
		return;

	//SET_PERSONALIZATION_PROPERTY(CObjectCreateToolPanel, "ObjectType", QVariant());
	Animate(m_stacked->widget(0), m_stacked->currentWidget(), true);
}

void CObjectCreateToolPanel::OnObjectTypeSelected(const char* type)
{
	//Here we assume that the widgets are always the same as the classes are static info, if this assumption changes then
	//we should still not regenerate all widgets but ask the class itself it we need to, because states would be lost.

	bool found = false;
	for (int i = 0; i < m_typeToStackIndex.size(); i++)
	{
		if (m_typeToStackIndex[i] == type)
		{
			QWidget* pNextWidget = m_stacked->widget(i);
			if (pNextWidget == m_stacked->currentWidget())
				return;

			QMenuComboBox* pComboBox = pNextWidget->findChild<QMenuComboBox*>();
			if (pComboBox)
				pComboBox->SetChecked(type);

			Animate(pNextWidget, m_stacked->currentWidget());

			found = true;
			break;
		}
	}

	if (!found)
	{
		if (QWidget* widgetToInsert = CreateWidgetOrStartCreate(type))
		{
			QWidget* pWidget = CreateWidgetContainer(widgetToInsert, type);
			m_typeToStackIndex.push_back(type);
			m_stacked->addWidget(pWidget);
			m_stacked->setAlignment(pWidget, Qt::AlignTop);
			Animate(pWidget, m_stacked->currentWidget());
		}
	}
}

void CObjectCreateToolPanel::Animate(QWidget* pIn, QWidget* pOut, bool bReverse /*= false*/)
{
	pIn->setGeometry(bReverse ? default_animation_distance : -default_animation_distance,
	                 pIn->pos().y(), pIn->width(), pIn->height());
	pIn->setVisible(true);

	QPropertyAnimation* pInAnim = new QPropertyAnimation(pIn, "geometry");
	QPointer<QPropertyAnimation> pInAnim_ptr = pInAnim;
	QMetaObject::Connection inDestroy = connect(pIn, &QObject::destroyed, [pInAnim_ptr]()
	{
		if (pInAnim_ptr)
		{
			pInAnim_ptr->disconnect();
			pInAnim_ptr->stop();
		}
	});
	pInAnim->setDuration(default_animation_time);
	pInAnim->setEasingCurve(QEasingCurve::OutCubic);
	pInAnim->setStartValue(QRectF(bReverse ? default_animation_distance : -default_animation_distance,
	                              pIn->pos().y(), pIn->width(), pIn->height()));
	pInAnim->setEndValue(QRectF(pIn->x(), pIn->y(), pIn->width(), pIn->height()));

	pInAnim->start(QAbstractAnimation::DeletionPolicy::DeleteWhenStopped);
	QPropertyAnimation* pOutAnim = new QPropertyAnimation(pOut, "geometry");
	QPointer<QPropertyAnimation> pOutAnim_ptr = pOutAnim;
	QMetaObject::Connection outDestroy = connect(pOut, &QObject::destroyed, [pOutAnim_ptr]()
	{
		if (pOutAnim_ptr)
		{
			pOutAnim_ptr->disconnect();
			pOutAnim_ptr->stop();
		}
	});
	pOutAnim->setDuration(default_animation_time);
	pOutAnim->setEasingCurve(QEasingCurve::InCubic);
	pOutAnim->setEndValue(QRectF(bReverse ? -default_animation_distance : default_animation_distance,
	                             pOut->pos().y(), pOut->width(), pOut->height()));
	pOutAnim->start(QAbstractAnimation::DeletionPolicy::DeleteWhenStopped);

	connect(pInAnim, &QPropertyAnimation::finished, [this, pIn, pOut, inDestroy, outDestroy]()
	{
		m_stacked->setCurrentWidget(pIn);
		pOut->setVisible(false);
		disconnect(inDestroy);
		disconnect(outDestroy);
	});
}

void CObjectCreateToolPanel::OnObjectSubTypeSelected(const char* type, QWidget* parent)
{
	QWidget* widgetToInsert = CreateWidgetOrStartCreate(type);
	if (widgetToInsert && parent)
	{
		QVBoxLayout* parentLayout = qobject_cast<QVBoxLayout*>(parent->layout());
		assert(parentLayout);
		while (parentLayout->count())
		{
			//assuming we will only find widgets in the layout
			auto item = parentLayout->itemAt(0);
			QWidget* w = item->widget();
			if (w)
			{
				w->deleteLater();
			}

			parentLayout->removeItem(item);
		}

		parentLayout->addWidget(widgetToInsert);
		if (!(widgetToInsert->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag))
			parentLayout->addStretch();
	}
}

QWidget* CObjectCreateToolPanel::CreateWidgetContainer(QWidget* pChild, const char* type)
{
	QWidget* pWidget = new QWidget(this);
	QVBoxLayout* pVertLayout = new QVBoxLayout();
	pWidget->setLayout(pVertLayout);
	pVertLayout->setSpacing(0);
	pVertLayout->setMargin(0);

	QPushButton* pButton = new QPushButton();
	pButton->setIcon(CryIcon("icons:General/Arrow_Left.ico"));
	pButton->setObjectName("back_btn");

	QMenuComboBox* pComboBox = new QMenuComboBox(this);
	pComboBox->SetMultiSelect(false);
	std::vector<string> categoryNames;
	GetIEditorImpl()->GetObjectManager()->GetClassCategories(categoryNames);
	for (int i = 0; i < categoryNames.size(); i++)
	{
		pComboBox->AddItem(QtUtil::ToQString(categoryNames[i]));
		if (categoryNames[i] == type)
			pComboBox->SetChecked(i);
	}

	std::function<void(int)> idxChangedFn = [this, pComboBox](int idx)
	{
		OnObjectTypeSelected(pComboBox->GetItem(idx).toStdString().c_str());
	};

	pComboBox->signalCurrentIndexChanged.Connect(idxChangedFn);

	connect(pButton, &QPushButton::clicked, this, &CObjectCreateToolPanel::OnBackButtonPressed);

	QHBoxLayout* pHorLayout = new QHBoxLayout();
	pHorLayout->setSpacing(0);
	pHorLayout->setMargin(0);
	pHorLayout->addWidget(pButton);
	pHorLayout->addWidget(pComboBox);

	pVertLayout->addLayout(pHorLayout);
	pVertLayout->addWidget(pChild);

	// If we are adding a new set of buttons as children then align top
	// if we do this with all widgets, trees are getting cut
	if (qobject_cast<CCreateObjectButtons*>(pChild))
		pVertLayout->setAlignment(pChild, Qt::AlignTop);

	// We need to intercept if the user presses the mouse back button
	QtUtil::RecursiveInstallEventFilter(this, pWidget);

	return pWidget;
}

QWidget* CObjectCreateToolPanel::CreateWidgetOrStartCreate(const char* type)
{
	QWidget* ret = nullptr;
	//Check if the selected type is a category
	std::vector<CObjectClassDesc*> subtypes;
	GetIEditorImpl()->GetObjectManager()->GetCreatableClassTypes(type, subtypes);

	// Save the object type only if it's a category
	/*if (subtypes.size() >= 1)
	{
		SET_PERSONALIZATION_PROPERTY(CObjectCreateToolPanel, "ObjectType", QVariant(type));
	}*/

	if (subtypes.size() > 1)
	{
		//Spawn widget to select subtype
		CCreateObjectButtons* buttons = new CCreateObjectButtons();

		for (CObjectClassDesc* subtype : subtypes)
		{
			// Name UI panel button with its UIName if available, but on click, work with the proper className
			buttons->AddButton(subtype->UIName(), [=]() { OnObjectSubTypeSelected(subtype->ClassName(), buttons); });
		}

		ret = buttons;
	}
	// fragile assumption: object with one subcategory usually include filtered lists.
	else if (subtypes.size() == 1)
	{
		CObjectClassDesc* clsDesc = GetIEditorImpl()->GetObjectManager()->FindClass(subtypes[0]->ClassName());
		if (clsDesc)
		{
			string filter = clsDesc->GetDataFilesFilterString();

			if (clsDesc->IsCreatedByListEnumeration())
			{
				if (filter.empty())
				{
					QObjectTreeWidget* pView = clsDesc->IsPreviewable() ? new CObjectCreateTreeWidget(clsDesc) : new QObjectTreeWidget();

					clsDesc->m_itemAdded.Connect(pView, &QObjectTreeWidget::AddEntry);
					clsDesc->m_itemChanged.Connect(pView, &QObjectTreeWidget::ChangeEntry);
					clsDesc->m_itemRemoved.Connect(pView, &QObjectTreeWidget::RemoveEntry);
					clsDesc->m_libraryRemoved.Connect([pView](const char* libName) { pView->RemoveEntry(libName, nullptr); });
					clsDesc->m_databaseCleared.Connect(pView, &QObjectTreeWidget::Clear);

					clsDesc->EnumerateObjects(pView);

					pView->signalOnDoubleClickFile.Connect([=](const char* selectedFile) { StartCreation(clsDesc, selectedFile); }, (intptr_t)this);
					pView->signalOnDragStarted.Connect([=](QDragEnterEvent* event, QDrag* drag)
					{
						StartCreation(clsDesc, nullptr);
					}, (intptr_t)this);
					pView->singalOnDragEnded.Connect(this, &CObjectCreateToolPanel::AbortCreateTool);
					connect(pView, &QObject::destroyed, [clsDesc, pView]()
					{
						clsDesc->m_itemAdded.DisconnectObject(pView);
						clsDesc->m_itemChanged.DisconnectObject(pView);
						clsDesc->m_itemRemoved.DisconnectObject(pView);
						clsDesc->m_libraryRemoved.DisconnectObject(pView);
						clsDesc->m_databaseCleared.DisconnectObject(pView);
					});
					ret = pView;
				}
				else if (filter.find(';') == -1 && filter.find('*') == -1)
				{
					//no filter or filter with no semicolon and wildcard, start creation directly
					StartCreation(clsDesc, filter);
				}
				else
				{
					auto browser = new CObjectBrowser(filter);
					browser->signalOnDoubleClickFile.Connect([=](const char* selectedFile) { StartCreation(clsDesc, selectedFile); }, (intptr_t)this);
					browser->signalOnDragStarted.Connect([=](QDragEnterEvent* event, QDrag* drag)
					{
						StartCreation(clsDesc, nullptr);
					}, (intptr_t)this);
					browser->singalOnDragEnded.Connect(this, &CObjectCreateToolPanel::AbortCreateTool);
					ret = browser;
					//TODO : add preview
				}
			}
			else
			{
				CCreateObjectButtons* buttons = new CCreateObjectButtons();
				ObjectButtonEnumerator objbuttons;
				// we use icons to create the object
				clsDesc->EnumerateObjects(&objbuttons);

				for (auto iter = objbuttons.GetEntries()->begin(); iter != objbuttons.GetEntries()->end(); ++iter)
				{
					string file = iter->second;
					// Name UI panel button with its UIName if available, but on click, work with the proper className
					buttons->AddButton(iter->first, [=]() { StartCreation(clsDesc, file); });
				}

				ret = buttons;
			}
		}
	}
	else
	{
		// handle items without sub-items here
		CObjectClassDesc* clsDesc = GetIEditorImpl()->GetObjectManager()->FindClass(type);
		if (clsDesc)
		{
			StartCreation(clsDesc, clsDesc->GetFileSpec());
		}
	}

	return ret;
}

void CObjectCreateToolPanel::StartCreation(CObjectClassDesc* cls, const char* file)
{
	if (!GetIEditorImpl()->GetDocument()->IsDocumentReady())
		return;

	if (strcmp(cls->GetToolClassName(), "EditTool.ObjectCreate") == 0)
	{
		CEditTool* editTool = GetIEditorImpl()->GetEditTool();
		CObjectCreateTool* objectCreateTool = editTool ? DYNAMIC_DOWNCAST(CObjectCreateTool, editTool) : nullptr;

		if (!objectCreateTool)
		{
			objectCreateTool = new CObjectCreateTool();
			GetIEditorImpl()->SetEditTool(objectCreateTool);
		}

		objectCreateTool->SelectObjectToCreate(cls, file);
	}
	else
	{
		GetIEditorImpl()->SetEditTool(cls->GetToolClassName(), true);
	}
}

void CObjectCreateToolPanel::AbortCreateTool()
{
	CEditTool* editTool = GetIEditorImpl()->GetEditTool();
	CObjectCreateTool* objectCreateTool = editTool ? DYNAMIC_DOWNCAST(CObjectCreateTool, editTool) : nullptr;

	if (objectCreateTool)
		objectCreateTool->Abort();
}

//////////////////////////////////////////////////////////////////////////

CCreateObjectButtons::CCreateObjectButtons(QWidget* pParent /*= nullptr*/)
	: QWidget(pParent)
	, m_layout(new QGridLayout())
{
	QVBoxLayout* pMainLayout = new QVBoxLayout();
	pMainLayout->setAlignment(Qt::AlignTop);
	setLayout(pMainLayout);

	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	pMainLayout->addLayout(m_layout);
}

CCreateObjectButtons::~CCreateObjectButtons()
{
}

void CCreateObjectButtons::AddButton(const char* szType, const std::function<void()>& onClicked)
{
	QString icon = QString("icons:CreateEditor/Add_To_Scene_%1.ico").arg(szType);
	icon.replace(" ", "_");

	auto pButton = new QToolButton(this);
	pButton->setText(QtUtil::CamelCaseToUIString(szType));
	if (QFile::exists(icon))
	{
		pButton->setIcon(CryIcon(icon));
	}
	pButton->setIconSize(QSize(24, 24));
	pButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	pButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	connect(pButton, &QToolButton::clicked, onClicked);

	int index = m_layout->count();
	int column = index % 2;
	int row = index / 2;
	m_layout->addWidget(pButton, row, column);
}

