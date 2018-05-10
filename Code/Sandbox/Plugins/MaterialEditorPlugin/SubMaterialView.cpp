// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "SubMaterialView.h"

#include "Cry3DEngine/IMaterial.h"

#include "MaterialEditor.h"
#include <QAbstractItemModel>

#include "Menu/MenuWidgetBuilders.h"
#include "QT/Widgets/QPreviewWidget.h"

class CSubMaterialView::Model : public QAbstractListModel
{
public:
	Model(QPreviewWidget* pPreviewWidget)
		: m_pMaterial(nullptr)
		, m_bIsMultiMtl(false)
	{
		CRY_ASSERT(pPreviewWidget);
		m_previewWidget = pPreviewWidget;
	}

	~Model()
	{
		if(m_pMaterial)
			m_pMaterial->signalSubMaterialsChanged.DisconnectObject(this);
	}

	void SetMaterial(CMaterial* pMaterial)
	{
		if (m_pMaterial != pMaterial)
		{
			if (m_pMaterial)
				m_pMaterial->signalSubMaterialsChanged.DisconnectObject(this);

			beginResetModel();
			m_pMaterial = pMaterial;
			if (m_pMaterial)
			{
				UpdateInternals();
				m_pMaterial->signalSubMaterialsChanged.Connect(this, &Model::OnSubMaterialChanged);
			}
			endResetModel();
		}
	}

	void InvalidateMaterial(CMaterial* pMaterial)
	{
		CRY_ASSERT(pMaterial && m_pMaterial);
		if (pMaterial == m_pMaterial && !m_bIsMultiMtl)
		{
			InvalidateRow(0);
		}
		else if (pMaterial->GetParent() == m_pMaterial)
		{
			const int count = m_pMaterial->GetSubMaterialCount();
			for (int i = 0; i < count; i++)
			{
				if (m_pMaterial->GetSubMaterial(i) == pMaterial)
				{
					InvalidateRow(i);
					break;
				}
			}
		}
	}

	inline CMaterial* GetMaterialForRow(int row) const
	{
		return m_bIsMultiMtl ? m_pMaterial->GetSubMaterial(row) : m_pMaterial;
	}

	//Use -1 for all rows
	void InvalidateRow(int row)
	{
		if (row == -1)
		{
			//invalidate all previews
			const int rowCount = this->rowCount(QModelIndex());
			if (rowCount >= 0)
			{
				//invalidate it
				for(int i = 0; i < rowCount; i++)
					m_pPreviewPixmaps[i] = RenderMaterial(GetMaterialForRow(i));

				dataChanged(index(0, 0, QModelIndex()), index(rowCount - 1, 0, QModelIndex()));
			}
		}
		else
		{
			//invalidate the right preview
			CRY_ASSERT(row < m_pPreviewPixmaps.size());
			m_pPreviewPixmaps[row] = RenderMaterial(GetMaterialForRow(row));

			const auto idx = index(row, 0, QModelIndex());
			dataChanged(idx, idx);
		}
	}

	void UpdateInternals()
	{
		m_bIsMultiMtl = m_pMaterial->IsMultiSubMaterial();

		if (m_bIsMultiMtl)
			m_pPreviewPixmaps.reserve(m_pMaterial->GetSubMaterialCount());

		m_pPreviewPixmaps.resize(m_bIsMultiMtl ? m_pMaterial->GetSubMaterialCount() : 1);

		InvalidateRow(-1);
	}

	void OnSubMaterialChanged(CMaterial::SubMaterialChange change)
	{
		switch (change)
		{
		case CMaterial::SlotCountChanged:
		case CMaterial::MaterialConverted:
			beginResetModel();
			UpdateInternals();
			endResetModel();
			break;
		case CMaterial::SubMaterialSet:
			InvalidateRow(-1);//all
			break;
		default:
			CRY_ASSERT(0);//case not handled
			break;
		}
	}

	virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override
	{
		if (!m_pMaterial)
			return 0;
		else
		{
			return m_bIsMultiMtl ? m_pMaterial->GetSubMaterialCount() : 1;
		}
	}

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override
	{
		if (row >= 0 && column >= 0)
		{
			//We're not using the submaterial pointer as id here because:
			// * it may be null in some cases
			// * we don't have a proper beginReset/endReset notification scheme, so beginReset may be called but the row count has already changed. 
			// * This ensures index() always returns valid index and the behavior observed is correct
			return createIndex(row, column, row);
		}
		else return QModelIndex();
	}

	virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		if(index.isValid())
		{
			const int row = index.row();

			CMaterial* material = GetMaterialForRow(row);

			switch (role)
			{
			case Qt::DisplayRole:
				if (material)
				{
					if (m_bIsMultiMtl)
						return QString(material->GetName());
					else
					{
						const auto pos = material->GetName().find_last_of('/');
						if(pos != string::npos)
							return QString(material->GetName().substr(pos + 1));
						else 
							return QString(material->GetName());
					}
				}
				else
					return "<null>";
				break;
			case Qt::DecorationRole:
				if (m_pPreviewPixmaps[row].isNull())
					return CryIcon("icons:common/assets_material.ico");
				else
					return QIcon(m_pPreviewPixmaps[row]);
			default:
				break;
			}
		}

		return QVariant();
	}

	virtual bool setData(const QModelIndex &index, const QVariant &value, int role /* = Qt::EditRole */) override
	{
		CRY_ASSERT(m_bIsMultiMtl && index.isValid());
		const int row = index.row();
		CMaterial* material = GetMaterialForRow(row);
		CRY_ASSERT(material && material->IsPureChild());

		const QString newName = value.toString();
		if (!newName.isEmpty())
		{
			material->GetParent()->RenameSubMaterial(material, newName.toStdString().c_str());
			return true;
		}

		return false;
	}

	virtual Qt::ItemFlags flags(const QModelIndex &index) const override
	{
		if (!m_bIsMultiMtl)
			return QAbstractListModel::flags(index);
		else if (index.isValid())
			return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
		else
			return QAbstractListModel::flags(index);
	}

	QPixmap RenderMaterial(CMaterial* pMaterial) const
	{
		if (!pMaterial)
			return QPixmap();

		//Generates a preview image for the material
		//Note: the render material does not do special handling for terrain material as the old one used to
		//This should be solved by the renderer and not the editor, as the editor should not know about material or shader specific handling

		//TODO: 
		// The preview widget should not have to be visible to be able to render a preview
		// The first frames of the preview sometimes do not show the material properly, I assume because the textures are not loaded properly
		m_previewWidget->SetMaterial(pMaterial);

		QImage image = m_previewWidget->GetImage();

		return QPixmap::fromImage(image);
	}

	QPreviewWidget* m_previewWidget;
	QVector<QPixmap> m_pPreviewPixmaps;
	CMaterial*  m_pMaterial;
	bool m_bIsMultiMtl;
};

CSubMaterialView::CSubMaterialView(CMaterialEditor* pMatEd)
	: QThumbnailsView()
	, m_pMatEd(pMatEd)
	, m_previewWidget(new QPreviewWidget())
	, m_pModel(new Model(m_previewWidget))
{
	//TODO: do we need configuration of the color and background here? 

	SetModel(m_pModel.get());

	auto view = GetInternalView();
	view->setSelectionMode(QAbstractItemView::SingleSelection);

	auto selectionModel = GetInternalView()->selectionModel();
	connect(selectionModel, &QItemSelectionModel::currentChanged, this, &CSubMaterialView::OnSelectionChanged);

	m_pMatEd->signalMaterialLoaded.Connect(this, &CSubMaterialView::OnMaterialChanged);
	m_pMatEd->signalMaterialForEditChanged.Connect(this, &CSubMaterialView::OnMaterialForEditChanged);
	m_pMatEd->signalMaterialPropertiesChanged.Connect(this, &CSubMaterialView::OnMaterialPropertiesChanged);

	view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(view, &QTreeView::customContextMenuRequested, this, &CSubMaterialView::OnContextMenu);

	QSize minSize, maxSize;
	GetItemSizeBounds(minSize, maxSize);

	m_previewWidget->SetGrid(false);
	m_previewWidget->SetAxis(false);
	m_previewWidget->EnableMaterialPrecaching(true);
	m_previewWidget->LoadFile("%EDITOR%/Objects/MtlPlane.cgf");
	m_previewWidget->SetCameraLookAt(1.0f, Vec3(0.0f, 0.0f, -1.0f));
	m_previewWidget->SetClearColor(QColor(0, 0, 0));
	m_previewWidget->setGeometry(0, 0, maxSize.width(), maxSize.height());

	m_previewWidget->setVisible(false); // hide this widget, it is just used to render to a pixmap
}

CSubMaterialView::~CSubMaterialView()
{
	m_pMatEd->signalMaterialLoaded.DisconnectObject(this);
	m_pMatEd->signalMaterialForEditChanged.DisconnectObject(this);
	m_pMatEd->signalMaterialPropertiesChanged.DisconnectObject(this);
}

void CSubMaterialView::OnMaterialChanged(CMaterial* pEditorMaterial)
{
	m_pModel->SetMaterial(pEditorMaterial);
}

void CSubMaterialView::OnMaterialForEditChanged(CMaterial* pEditorMaterial)
{
	if (!m_pModel->m_pMaterial)
		return;

	if (!pEditorMaterial)
	{
		GetInternalView()->selectionModel()->clear();
		return;
	}

	if (!m_pModel->m_pMaterial->IsMultiSubMaterial())
	{
		CRY_ASSERT(pEditorMaterial == m_pModel->m_pMaterial);
	}
	else
	{
		//Select in the view unless already selected
		const auto index = GetInternalView()->selectionModel()->currentIndex();
		const int row = index.row();

		if (!index.isValid() || m_pModel->m_pMaterial->GetSubMaterial(row) != pEditorMaterial)
		{
			const int subMtlCount = m_pModel->m_pMaterial->GetSubMaterialCount();
			CRY_ASSERT(row < subMtlCount);

			for (int i = 0; i < subMtlCount; i++)
			{
				if (m_pModel->m_pMaterial->GetSubMaterial(i) == pEditorMaterial)
				{
					GetInternalView()->selectionModel()->select(m_pModel->index(i, 0), QItemSelectionModel::ClearAndSelect);
					return;
				}
			}
		}
	}
}

void CSubMaterialView::OnMaterialPropertiesChanged(CMaterial* pEditorMaterial)
{
	if (m_pModel->m_pMaterial && (m_pModel->m_pMaterial == pEditorMaterial || pEditorMaterial->GetParent() == m_pModel->m_pMaterial))
		m_pModel->InvalidateMaterial(pEditorMaterial);
}

void CSubMaterialView::OnSelectionChanged(const QModelIndex& current, const QModelIndex& previous)
{
	if (!m_pModel->m_pMaterial)
		return;

	if (!current.isValid())
	{
		m_pMatEd->SelectMaterialForEdit(nullptr);
		return;
	}

	const int row = current.row();
	if (m_pModel->m_pMaterial->IsMultiSubMaterial())
	{
		CRY_ASSERT(m_pModel->m_pMaterial->GetSubMaterialCount() > row);
		m_pMatEd->SelectMaterialForEdit(m_pModel->m_pMaterial->GetSubMaterial(row));
	}
	else
	{
		CRY_ASSERT(row == 0);
		m_pMatEd->SelectMaterialForEdit(m_pModel->m_pMaterial);
	}
	
}

void CSubMaterialView::OnContextMenu(const QPoint& pos)
{
	QModelIndex index = GetInternalView()->indexAt(pos);

	CMaterial* pMtl = m_pModel->m_pMaterial;
	if (!pMtl)
		return;

	auto menu = new QMenu();

	int row = -1;
	if (index.isValid())
	{
		row = index.row();
	}


	bool bIsMultiMtl = pMtl->IsMultiSubMaterial();
	CMaterial* pSelectedMaterial = nullptr;

	if(bIsMultiMtl)
		pSelectedMaterial = row == -1 ? pMtl : pMtl->GetSubMaterial(row);
	else
	{
		CRY_ASSERT(row == 0 || row == -1);
		pSelectedMaterial = pMtl;
	}

	const bool isHighlighted = GetIEditor()->GetMaterialManager()->GetHighlightedMaterial() == pSelectedMaterial;
	//Highlight
	//TODO: this could also be a global material editor menu/toolbar action
	if(!isHighlighted)
	{
		auto action = menu->addAction(tr("Highlight"));
		connect(action, &QAction::triggered, [this, pSelectedMaterial]() {
			GetIEditor()->GetMaterialManager()->SetHighlightedMaterial(pSelectedMaterial);
		});
	}
	else
	{
		auto action = menu->addAction(tr("Stop Highlighting"));
		connect(action, &QAction::triggered, [this, pSelectedMaterial]() {
			GetIEditor()->GetMaterialManager()->SetHighlightedMaterial(0);
		});
	}

	menu->addSeparator();

	//Abstract menu must not go out of scope for actions to be visible in the menu
	CAbstractMenu materialMenu;

	const bool isReadOnly = m_pMatEd->IsReadOnly();
	if(!isReadOnly)
	{
		if (row != -1 && bIsMultiMtl)
		{
			//Menu of the selected sub material
			auto action = menu->addAction(tr("Rename Sub-Material"));
			connect(action, &QAction::triggered, [this, index]() {
				GetInternalView()->edit(index);
			});

			action = menu->addAction(tr("Reset Sub-Material"));
			connect(action, &QAction::triggered, [this, row]() {
				m_pMatEd->OnResetSubMaterial(row);
			});

			action = menu->addAction(tr("Remove Sub-Material"));
			connect(action, &QAction::triggered, [this, row]() {
				m_pMatEd->OnRemoveSubMaterial(row);
			});

			menu->addSeparator();
		}

		//Add global material menu options
		
		m_pMatEd->FillMaterialMenu(&materialMenu);
		MenuWidgetBuilders::CMenuBuilder::FillQMenu(menu, &materialMenu);
	}

	menu->exec(QCursor::pos());
}

void CSubMaterialView::customEvent(QEvent* event)
{
	if (event->type() == SandboxEvent::Command)
	{
		CommandEvent* commandEvent = static_cast<CommandEvent*>(event);

		const string& command = commandEvent->GetCommand();
		if (command == "general.delete")
		{
			const auto selectedIdx = GetInternalView()->selectionModel()->currentIndex();
			if (selectedIdx.isValid())
				m_pMatEd->OnRemoveSubMaterial(selectedIdx.row());
			event->accept();
		}
	}
}


