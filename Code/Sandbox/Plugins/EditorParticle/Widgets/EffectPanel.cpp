// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "EffectPanel.h"

#include "../Models/EffectAssetModel.h"
#include "IUndoObject.h"
#include <EditorFramework/InspectorLegacy.h>
#include <EditorFramework/BroadcastManager.h>
#include <QAdvancedPropertyTreeLegacy.h>
#include <QCollapsibleFrame.h>
#include <QTreeWidget.h>
#include <QListWidget.h>
#include <QStandardItemModel.h>

/////////////////////////////////////////////////////////////////////////
QWidget* LabeledWidget(QWidget* child, const char* text)
{
	auto widget = new QWidget(child->parentWidget());
	auto layout = new QVBoxLayout(widget);
	auto label = new QLabel(text);
	label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	layout->addWidget(label);
	layout->addWidget(child);
	return widget;
}

/////////////////////////////////////////////////////////////////////////
// Implement editing commands, with undo/redo
namespace Command
{
	template<class TOwner>
	class TCommand : public IUndoObject
	{
	public:
		TCommand(TOwner& owner, bool forward, const string& desc = string())
			: m_owner(owner), m_forward(forward), m_description(desc) {}

		virtual bool Execute(int mode) = 0;

		// IUndoObject
		const char* GetDescription() final { return m_description.c_str(); }
		void Redo() final { Execute(1); }
		void Undo(bool b) final { if (b) Execute(-1); }

	protected:
		TOwner& m_owner;
		bool    m_forward;
		string  m_description;
	};

	template<class TCommand, class ... Args>
	void Dispatch(Args&& ... args)
	{
		IUndoManager& undo = *GetIEditor()->GetIUndoManager();
		if (undo.IsUndoTransaction())
			return;

		TCommand* command = new TCommand(std::forward<Args>(args)...);
		if (command->Execute(0))
		{
			undo.Begin();
			undo.RecordUndo(command);
			undo.Accept(command->GetDescription());
		}
	}
}

/////////////////////////////////////////////////////////////////////////
namespace CryParticleEditor {

template<typename T>
bool EnableObject(T* pObject, bool enable)
{ 
	if (pObject->IsEnabled() == enable)
		return false;
	pObject->SetEnabled(enable);
	return true;
}

template<typename T, typename Item>
bool RenameObject(T* pObject, Item* item)
{ 
	auto itemName = item->text(0).toStdString();
	auto objName = pObject->GetName();
	if (itemName == objName)
		return false;
	pObject->SetName(itemName.c_str());
	objName = pObject->GetName();
	if (itemName != objName)
		item->setText(0, objName);
	return true;
}

void EnableMove(QAbstractItemView* w)
{
	w->setDragEnabled(true);
	w->setDragDropMode(QAbstractItemView::InternalMove);
	w->setDefaultDropAction(Qt::MoveAction);
	w->setDropIndicatorShown(true);
}

/////////////////////////////////////////////////////////////////////////
class CFeatureList: public QListWidget
{
public:
	CFeatureList(QWidget* parent)
		: QListWidget(parent)
	{
		CreateFeatureMenu();

		// Create Component view (list of Features)
		setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
		setSelectionMode(QAbstractItemView::ExtendedSelection);
		setEditTriggers(QAbstractItemView::NoEditTriggers);

		EnableMove(this);

		// Select
		connect(this, &QListWidget::itemSelectionChanged, this,
			[this]()
		{
			InspectFeatures(false);
		});

		// Enable/disable
		connect(this, &QListWidget::itemChanged, this,
			[this](QListWidgetItem* item)
		{
			uint index = row(item);
			bool enable = item->checkState() == Qt::Checked;
			Command::Dispatch<CEnableCommand>(*this, index, enable);
			InspectFeatures(true);
		});

		// Remove items
		connect(model(), &QStandardItemModel::rowsRemoved, this,
			[this](const QModelIndex&, int first, int last)
		{
			for (int i = first; i <= last; ++i)
				Command::Dispatch<CAddRemoveCommand>(*this, i);
			InspectFeatures(true);
		});

		// Move items
		connect(model(), &QStandardItemModel::rowsMoved, this,
			[this](const QModelIndex&, int first, int last, const QModelIndex&, int dest)
		{
			for (int i = first; i <= last; ++i)
				Command::Dispatch<CMoveCommand>(*this, i, dest);
		});

		// Delete action: 2do: implement shortcuts, via subclassing: CEffectAssetWidget::customEvent
		auto pRemoveAction = new QAction("Remove", this);
		pRemoveAction->setShortcuts(QKeySequence::Delete);
		pRemoveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(pRemoveAction, &QAction::triggered, this, [this]()
		{
			auto selected = selectedItems();
			for (auto* item: selected)
			{
				uint index = row(item);
				takeItem(index);
				delete item;
			}
		});
		insertAction(0, pRemoveAction);

		connect(m_pFeatureMenu, &QMenu::triggered, this, [this](QAction* pAction)
		{
			if (size_t addr = pAction->property("FeatureParam").value<size_t>())
			{
				uint pos = selectedItems().size() ? currentRow() : count();
				const auto& params = *(const SParticleFeatureParams*)addr;
				Command::Dispatch<CAddRemoveCommand>(*this, pos, params);
			}
		});

		// Context menu
		setContextMenuPolicy(Qt::CustomContextMenu);

		connect(this, &QListWidget::customContextMenuRequested, this,
			[this, pRemoveAction](const QPoint&)
		{
			QMenu contextMenu(this);

			contextMenu.addMenu(m_pFeatureMenu);
			if (selectedItems().size())
			{
				contextMenu.addAction(pRemoveAction);
				m_pFeatureMenu->setTitle("Insert");
			}
			else
				m_pFeatureMenu->setTitle("Append");
			contextMenu.exec(QCursor::pos());
		});
	}

	void SetComponent(IParticleComponent* pComp)
	{
		clear();
		m_pComponent = pComp;
		m_featuresSelected.m_pComponent = pComp;
		if (!pComp)
			return;

		uint num = pComp->GetNumFeatures();
		for (uint i = 0; i < num; ++i)
			AddFeatureItem(i);

		update();
		InspectFeatures(true);
	}

	void CreateFeatureMenu()
	{
		// Populate and sort feature params
		const uint numParams = GetParticleSystem()->GetNumFeatureParams();
		std::vector<const SParticleFeatureParams*> featureParams;
		featureParams.reserve(numParams);
		for (uint i = 0; i < numParams; ++i)
			featureParams.push_back(&GetParticleSystem()->GetFeatureParam(i));
		std::sort(featureParams.begin(), featureParams.end(), [](const SParticleFeatureParams* a, const SParticleFeatureParams* b)
		{
			return strcmp(a->m_fullName, b->m_fullName) < 0;
		});

		// Populate menu with plain actions
		m_pFeatureMenu = new QMenu("Insert", this);

		cstr curGroup = "";
		QMenu* pSubMenu = nullptr;
		for (auto const* pf : featureParams)
		{
			if (strcmp(pf->m_groupName, curGroup) != 0)
			{
				// New sub-menu
				pSubMenu = m_pFeatureMenu->addMenu(curGroup = pf->m_groupName);
			}
			QAction* pAction = pSubMenu->addAction(pf->m_featureName);
			pAction->setProperty("FeatureParam", QVariant(size_t(pf)));
		}
	}

	void AddFeatureItem(uint index)
	{
		IParticleFeature* pFeature = m_pComponent->GetFeature(index);
		const auto& params = pFeature->GetFeatureParams();

		auto item = new QListWidgetItem(pFeature->GetFeatureParams().m_fullName);
		item->setCheckState(pFeature->IsEnabled() ? Qt::Checked : Qt::Unchecked);

		QBrush fg = item->foreground();
		QColor color(params.m_color.r, params.m_color.g, params.m_color.b);
		fg.setColor(color);
		item->setForeground(fg);

		insertItem(index, item);
	}

	void InspectFeatures(bool bAll)
	{
		assert(m_pComponent);
		cstr title = "";
		m_featuresSelected.clear();

		uint num = m_pComponent->GetNumFeatures();
		for (uint i = 0; i < num; ++i)
		{
			if (!bAll && !item(i)->isSelected())
				continue;
			IParticleFeature* feature = m_pComponent->GetFeature(i);
			if (bAll && !feature->IsEnabled())
				continue;
			m_featuresSelected.push_back(feature);
			if (!*title)
				title = feature->GetFeatureParams().m_fullName;
			else
				title = bAll ? m_pComponent->GetName() : "Features";
		}

		Inspect(Serialization::SStruct(m_featuresSelected), title);
	}

	void Inspect(const Serialization::SStruct& serializer, cstr title)
	{
		if (CBroadcastManager* pBroadcastManager = CBroadcastManager::Get(this))
		{
			auto* propertyTree = new QAdvancedPropertyTreeLegacy(title, this);

			propertyTree->setSizeToContent(true);
			auto style = propertyTree->treeStyle();
			style.propertySplitter = false;
			style.doNotIndentSecondLevel = false;
			propertyTree->setTreeStyle(style);
			propertyTree->setExpandLevels(4);

			propertyTree->attach(serializer);

			auto callback = [propertyTree](CInspectorLegacy& inspector) { inspector.AddWidget(propertyTree); };
			pBroadcastManager->Broadcast(PopulateLegacyInspectorEvent(callback, title));

			QObject::connect(propertyTree, &QPropertyTreeLegacy::signalChanged, [this]()
			{
				signalEdited(m_pComponent->GetIndex());
			});
		}
	}

	CCrySignal<void(int)> signalEdited;

private:

	struct TFeatures : std::vector<IParticleFeature*>
	{
		_smart_ptr<IParticleComponent>  m_pComponent;

		bool Serialize(Serialization::IArchive& ar)
		{
			Serialization::SContext compContext(ar, &*m_pComponent);

			if (size() == 1)
			{
				front()->Serialize(ar);
			}
			else for (auto* pf: *this)
			{
				cstr name = pf->GetFeatureParams().m_fullName;
				ar.openBlock(name, name);
				pf->Serialize(ar);
				ar.closeBlock();
			}
			if (ar.isInput())
				m_pComponent->SetChanged();

			return true;
		}
	};

	_smart_ptr<IParticleComponent> m_pComponent;
	QMenu*                         m_pFeatureMenu;
	TFeatures                      m_featuresSelected;

	/////////////////////////////////////////////////////////////////////////
	// Commands
	struct CFeatureCommand: Command::TCommand<CFeatureList>
	{
		uint m_index;

		CFeatureCommand(CFeatureList& view, uint index, bool forward)
			: Command::TCommand<CFeatureList>(view, forward), m_index(index)
		{
		}
		IParticleFeature& Feature() const
		{
			return *m_owner.m_pComponent->GetFeature(m_index);
		}
	};

	struct CEnableCommand: CFeatureCommand
	{
		CEnableCommand(CFeatureList& view, uint index, bool enable)
			: CFeatureCommand(view, index, enable)
		{
			m_description = enable ? "Enable " : "Disable ";
			m_description += "feature ";
			m_description += Feature().GetFeatureParams().m_fullName;
		}

		bool Execute(int mode) override
		{
			bool enable = m_forward ^ (mode < 0);
			if (!EnableObject(&Feature(), enable))
				return false;
			m_owner.m_pComponent->SetChanged();
			if (mode != 0)
				m_owner.item(m_index)->setCheckState(enable ? Qt::Checked : Qt::Unchecked);
			m_owner.signalEdited(m_index);
			return true;
		}
	};

	struct CAddRemoveCommand: CFeatureCommand
	{
		const pfx2::SParticleFeatureParams& m_params;
		DynArray<char>                      m_data;

		CAddRemoveCommand(CFeatureList& view, uint index, const pfx2::SParticleFeatureParams& params)
			: CFeatureCommand(view, index, true), m_params(params)
		{
			m_description = "Add feature ";
			m_description += m_params.m_fullName;
		}

		CAddRemoveCommand(CFeatureList& view, uint index)
			: CFeatureCommand(view, index, false), m_params(Feature().GetFeatureParams())
		{
			Serialization::SaveJsonBuffer(m_data, *this);
			m_data.push_back(0);
			m_description = "Remove feature ";
			m_description += m_params.m_fullName;
		}

		void Serialize(Serialization::IArchive& ar)
		{
			auto& component = *m_owner.m_pComponent;
			Serialization::SContext context(ar, &component);
			Feature().Serialize(ar);
		}

		bool Execute(int mode) override
		{
			bool adding = m_forward ^ (mode < 0);
			if (adding)
			{
				m_owner.m_pComponent->AddFeature(m_index, m_params);
				Serialization::LoadJsonBuffer(*this, m_data.data(), m_data.size());
				m_owner.AddFeatureItem(m_index);
			}
			else
			{
				m_owner.m_pComponent->RemoveFeature(m_index);
				if (mode != 0)
					m_owner.takeItem(m_index);
			}
			m_owner.signalEdited(-1);
			return true;
		}
	};

	struct CMoveCommand: CFeatureCommand
	{
		uint m_dest;

		CMoveCommand(CFeatureList& view, uint source, uint dest)
			: CFeatureCommand(view, source, true), m_dest(dest)
		{
			m_description = "Move feature ";
			m_description += Feature().GetFeatureParams().m_fullName;
		}

		bool Execute(int mode) override
		{
			uint src = mode >= 0 ? m_index : m_dest;
			uint dst = mode >= 0 ? m_dest : m_index;
			dst -= dst > src;

			DynArray<uint, uint> ids;
			ids.resize(m_owner.m_pComponent->GetNumFeatures());
			for (uint i = 0; i < ids.size(); ++i)
				ids[i] = i;
			ids.erase(src);
			ids.insert(dst, src);

			m_owner.m_pComponent->SwapFeatures(&ids[0], ids.size());
			if (mode != 0)
			{
				auto item = m_owner.takeItem(src);
				m_owner.insertItem(dst, item);
			}
			m_owner.signalEdited(-1);
			return true;
		}
	};
};

/////////////////////////////////////////////////////////////////////////
class CComponentTree: public QTreeWidget
{
public:
	CComponentTree::CComponentTree(QWidget* parent)
		: QTreeWidget(parent)
	{
		// Create Effect view (tree of Components)
		setColumnCount(1);
		setUniformRowHeights(true);
		setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
		setSelectionMode(QAbstractItemView::ExtendedSelection);
		EnableMove(this);

		setHeaderLabel("Components");

		// Enable/disable and rename
		connect(this, &QTreeWidget::itemChanged, this,
			[this](QTreeWidgetItem* item, int column)
		{
			Command::Dispatch<CEnableCommand>(*this, item, item->checkState(0) == Qt::Checked);
			Command::Dispatch<CRenameCommand>(*this, item);
		});

		// Menu Actions
		auto pVisibleAction = new QAction("Visible", this);
		pVisibleAction->setCheckable(true);
		connect(pVisibleAction, &QAction::triggered, this, [this]()
		{
			auto selected = selectedItems();
			for (auto* item: selected)
			{
				Command::Dispatch<CVisibleCommand>(*this, item, -1);
			}
		});
		insertAction(0, pVisibleAction);

		auto pSoloAction = new QAction("Solo Visible", this);
		connect(pSoloAction, &QAction::triggered, this, [this]()
		{
			if (auto* itemCur = currentItem())
			{
				for (QTreeWidgetItem* item = topLevelItem(0); item; item = NextItem(item))
				{
					Command::Dispatch<CVisibleCommand>(*this, item, item == itemCur);
				}
			}
			signalEdited(-1);
		});
		insertAction(0, pSoloAction);

		auto pRemoveAction = new QAction("Remove (with children)", this);
		pRemoveAction->setShortcuts(QKeySequence::Delete);
		pRemoveAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		connect(pRemoveAction, &QAction::triggered, this, [this]()
		{
			// Remove rows (and children) in reverse order
			auto selected = selectedItems();
			std::vector<int> rows;
			for (QTreeWidgetItem* item: selected)
			{
				// Find extent of selected subtree
				QTreeWidgetItem* subItem = item;
				while (subItem->childCount())
					subItem = subItem->child(subItem->childCount() - 1);
				int row = RowFromItem(item);
				int subRow = RowFromItem(subItem);
				for (; row <= subRow; ++row)
					stl::binary_insert_unique(rows, -row);
			}
			for (int row : rows)
				Command::Dispatch<CRemoveCommand>(*this, -row);
		});
		insertAction(0, pRemoveAction);

		connect(m_pComponentMenu, &QMenu::triggered, this, [this](QAction* pAction)
		{
			cstr asset = pAction->property("asset").toString().toStdString().c_str();
			if (*asset)
			{
				string name = pAction->text().toStdString().c_str();
				auto parentItem = selectedItems().size() ? currentItem() : nullptr;
				int row = RowFromItem(parentItem);
				Command::Dispatch<CAddCommand>(*this, row, name, asset);
			}
		});

		// Context menu
		// setContextMenuPolicy(Qt::ActionsContextMenu);
		setContextMenuPolicy(Qt::CustomContextMenu);

		connect(this, &QTreeWidget::customContextMenuRequested, this,
			[this, pVisibleAction, pSoloAction, pRemoveAction](const QPoint&)
		{
			if (!m_pEffect)
				return;
			QMenu contextMenu(this);
			if (selectedItems().size())
			{
				if (selectedItems().size() == 1)
				{
					if (IParticleComponent* pComp = ComponentFromItem(currentItem()))
					{
						if (pComp->CanBeParent())
						{
							contextMenu.addMenu(m_pComponentMenu);
							m_pComponentMenu->setTitle("Add Child Component");
						}

						pVisibleAction->setChecked(pComp->IsVisible());
						bool isSoloVisible = pComp->IsVisible();
						if (isSoloVisible)
						{
							for (uint i = m_pEffect->GetNumComponents(); i-- > 0; )
							{
								IParticleComponent* pC = m_pEffect->GetComponent(i);
								if (pC != pComp && pC->IsVisible())
								{
									isSoloVisible = false;
									break;
								}
							}
						}
						if (!isSoloVisible)
							contextMenu.addAction(pSoloAction);
					}
				}
				contextMenu.addAction(pRemoveAction);
				contextMenu.addAction(pVisibleAction);
			}
			else
			{
				contextMenu.addMenu(m_pComponentMenu);
				m_pComponentMenu->setTitle("Add Component");
			}
			contextMenu.exec(QCursor::pos());
		});
	}

	void SetEffect(IParticleEffect* pEffect)
	{
		clear();
		m_pEffect = pEffect;
		if (!pEffect)
			return;

		string name = pEffect->GetName();
		if (name.Right(4).MakeLower() == ".pfx")
			name.resize(name.length() - 4);
		if (name.Left(10).MakeLower() == "particles/")
			name.erase(0, 10);

		setHeaderLabel(QString(name));

		// Add components to tree, determining proper parent. Components are always sorted by parenthood.
		QTreeWidgetItem* pParentItem = nullptr;

		uint num = pEffect->GetNumComponents();
		uint level = 0;
		for (uint i = 0; i < num; ++i)
		{
			IParticleComponent* pComp = pEffect->GetComponent(i);
			if (IParticleComponent* pParentComp = pComp->GetParent())
			{
				if (pParentComp == pEffect->GetComponent(i - 1))
					level++;
				else
				{
					uint newLevel = 1;
					while (pParentComp = pParentComp->GetParent())
						newLevel++;
					while (level > newLevel)
					{
						level--;
						pParentItem = pParentItem->parent();
					}
					pParentItem = pParentItem->parent();
				}
			}
			else
			{
				pParentItem = nullptr;
				level = 0;
			}

			pParentItem = InsertComponent(pComp, pParentItem);
		}

		expandAll();
	}

	int RowFromItem(QTreeWidgetItem* item) const
	{
		int row = 0;
		for (QTreeWidgetItem* elem = topLevelItem(0); elem; elem = NextItem(elem))
		{
			if (elem == item)
				return row;
			row++;
		}
		return -1;
	}

	IParticleComponent* ComponentFromItem(QTreeWidgetItem* item) const
	{
		uint row = RowFromItem(item);
		return row < m_pEffect->GetNumComponents() ? m_pEffect->GetComponent(row) : 0;
	}

	CCrySignal<void(int)> signalEdited;

private:
	_smart_ptr<IParticleEffect> m_pEffect;
	QMenu*                          m_pComponentMenu = CreateComponentMenu();

	void dropEvent(QDropEvent *event) override
	{
		auto index = indexAt(event->pos());
		auto* newParent = ComponentFromItem(itemFromIndex(index));
		if (newParent && !newParent->CanBeParent())
		{
			event->setDropAction(Qt::IgnoreAction);
			return;
		}

		for (auto source : selectedItems())
		{
			if (auto* comp = ComponentFromItem(source))
			{
				Command::Dispatch<CMoveCommand>(*this, source, newParent, -1);
			}
		}
		QTreeWidget::dropEvent(event);
	}

	QMenu* CreateComponentMenu()
	{
		QMenu* menu = new QMenu("Add Component", this);

		CNodesDictionary dict;
		for (int e = 0, num = dict.GetNumEntries(); e < num; ++e)
		{
			AddEntry(menu, dict.GetEntry(e));
		}
		return menu;
	}

	void AddEntry(QMenu* menu, const CAbstractDictionaryEntry* entry)
	{
		QString name = entry->GetColumnValue(0).toString();

		if (int num = entry->GetNumChildEntries())
		{
			// Sub-menu
			QMenu* subMenu = menu->addMenu(name);
			for (int e = 0; e < num; ++e)
			{
				AddEntry(subMenu, entry->GetChildEntry(e));
			}
		}
		else
		{
			// Action
			QAction* action = menu->addAction(name);
			QString asset = entry->GetIdentifier().toString();
			action->setProperty("asset", asset);
		}
	};

	QTreeWidgetItem* NextItem(QTreeWidgetItem* item) const
	{
		if (item->childCount())
			return item->child(0);
		while (item->parent())
		{
			int index = item->parent()->indexOfChild(item) + 1;
			if (index < item->parent()->childCount())
				return item->parent()->child(index);
			item = item->parent();
		}
		int index = indexOfTopLevelItem(item) + 1;
		if (index < topLevelItemCount())
			return topLevelItem(index);
		return nullptr;
	}

	QTreeWidgetItem* InsertComponent(IParticleComponent* pComp, QTreeWidgetItem* parentItem)
	{
		auto item = new QTreeWidgetItem(); // parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem(this);
		item->setText(0, pComp->GetName());
		item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled);
		if (pComp->CanBeParent())
			item->setFlags(item->flags() | Qt::ItemIsDropEnabled);
		item->setCheckState(0, pComp->IsEnabled() ? Qt::Checked : Qt::Unchecked);
		item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
		SetItemVisible(item, pComp->IsVisible());

		if (parentItem)
			parentItem->addChild(item);
		else
			addTopLevelItem(item);
		return item;
	}

	void SetItemVisible(QTreeWidgetItem *item, bool visible)
	{
		// Invisible components are shown dimmed
		QBrush fg = item->foreground(0);
		QColor color = fg.color();
		color.setRgbF(1, 1, 1, visible ? 1.0 : 0.5);
		fg.setColor(color);
		item->setForeground(0, fg);
	}

	/////////////////////////////////////////////////////////////////////////
	// Commands
	struct CComponentCommand: Command::TCommand<CComponentTree>
	{
		QTreeWidgetItem*    m_item;
		IParticleComponent& m_component;

		CComponentCommand(CComponentTree& view, QTreeWidgetItem* item, bool forward = true)
			: Command::TCommand<CComponentTree>(view, forward), m_item(item), m_component(*m_owner.ComponentFromItem(m_item))
		{
		}

		void SetChanged() const
		{
			m_component.SetChanged();
			m_owner.signalEdited(m_owner.RowFromItem(m_item));
		}
		void SetEffectChanged() const
		{
			m_owner.signalEdited(-1);
			m_owner.SetEffect(m_owner.m_pEffect);
		}
	};

	struct CEnableCommand: CComponentCommand
	{
		CEnableCommand(CComponentTree& view, QTreeWidgetItem* item, bool enable)
			: CComponentCommand(view, item, enable)
		{
			m_description = enable ? "Enable " : "Disable ";
			m_description += "component ";
			m_description += m_component.GetName();
		}
		bool Execute(int mode) override
		{
			bool enable = m_forward ^ (mode < 0);
			if (!EnableObject(&m_component, enable))
				return false;
			if (mode != 0)
				m_item->setCheckState(0, enable ? Qt::Checked : Qt::Unchecked);
			SetChanged();
			return true;
		}
	};

	struct CVisibleCommand: CComponentCommand
	{
		CVisibleCommand(CComponentTree& view, QTreeWidgetItem* item, int visible)
			: CComponentCommand(view, item)
		{
			m_forward = visible >= 0 ? visible : !m_component.IsVisible();
			m_description = m_forward ? "Set visible " : "Set invisible ";
			m_description += "component ";
			m_description += m_component.GetName();
		}
		bool Execute(int mode) override
		{
			bool visible = m_forward ^ (mode < 0);
			if (visible == m_component.IsVisible())
				return false;
			m_component.SetVisible(visible);
			m_owner.SetItemVisible(m_item, visible);
			SetChanged();
			return true;
		}
	};

	struct CRenameCommand: CComponentCommand
	{
		CRenameCommand(CComponentTree& view, QTreeWidgetItem* item)
			: CComponentCommand(view, item), m_oldName(m_component.GetName())
		{
			m_description = "Rename component ";
			m_description += m_oldName;
		}
		bool Execute(int mode) override
		{
			if (mode == 0)
			{
				if (!RenameObject(&m_component, m_item))
					return false;
				m_newName = m_component.GetName();
			}
			else
			{
				m_component.SetName(mode < 0 ? m_oldName : m_newName);
				m_item->setText(0, m_component.GetName());
			}
			SetChanged();
			return true;
		}

		string m_oldName, m_newName;
	};

	struct CMoveCommand: CComponentCommand
	{
		CMoveCommand(CComponentTree& view, QTreeWidgetItem* item, IParticleComponent* newParent, int newIndex)
			: CComponentCommand(view, item), m_newParent(newParent), m_newIndex(newIndex)
		{
			m_description = "Move component ";
			m_description += m_component.GetName();
			m_oldParent = m_component.GetParent();
			m_oldIndex = m_component.GetIndex(true);
		}
		bool Execute(int mode) override
		{
			if (mode >= 0)
			{
				if (!m_component.SetParent(m_newParent, m_newIndex))
					return false;
			}
			else
				m_component.SetParent(m_oldParent, m_oldIndex);
			SetEffectChanged();
			return true;
		}

		IParticleComponent* m_oldParent;
		IParticleComponent* m_newParent;
		int m_oldIndex, m_newIndex;
	};

	struct CRemoveCommand: Command::TCommand<CComponentTree>
	{
		CRemoveCommand(CComponentTree& view, int row)
			: Command::TCommand<CComponentTree>(view, false), m_index(row)
		{
			IParticleComponent* pComp = m_owner.m_pEffect->GetComponent(row);
			m_description = "Remove component ";
			m_description += pComp->GetName();
			Serialization::SaveJsonBuffer(m_saveComponent, *pComp);
			m_saveComponent.push_back(0);
			m_parentIndex = pComp->GetParent() ? pComp->GetParent()->GetIndex() : -1;
			m_parentPos = pComp->GetIndex(true);
		}

		bool Execute(int mode) override
		{
			if (mode >= 0)
			{
				m_owner.m_pEffect->RemoveComponent(m_index, true);
			}
			else
			{
				IParticleComponent* pComp = m_owner.m_pEffect->AddComponent();
				Serialization::LoadJsonBuffer(*pComp, m_saveComponent.data(), m_saveComponent.size());
				pComp->SetParent(m_owner.m_pEffect->GetComponent(m_parentIndex), m_parentPos);
			}
			m_owner.signalEdited(-1);
			m_owner.SetEffect(m_owner.m_pEffect);
			return true;
		}

		int            m_index;
		int            m_parentIndex, m_parentPos;
		DynArray<char> m_saveComponent;
	};

	struct CAddCommand: Command::TCommand<CComponentTree>
	{
		CAddCommand(CComponentTree& view, int parent, cstr name, cstr asset)
			: Command::TCommand<CComponentTree>(view, true), m_parentIndex(parent), m_name(name), m_asset(asset)
		{
			m_description = "Add component ";
			m_description += name;
		}

		bool Execute(int mode) override
		{
			if (mode >= 0)
			{
				IParticleComponent* pComp = m_owner.m_pEffect->AddComponent();
				if (Serialization::LoadJsonFile(*pComp, m_asset))
				{
					pComp->SetName(m_name);
					if (m_parentIndex >= 0)
						pComp->SetParent(m_owner.m_pEffect->GetComponent(m_parentIndex));
					m_owner.m_pEffect->Update();
					m_index = pComp->GetIndex();
				}
				else
				{
					m_owner.m_pEffect->RemoveComponent(m_owner.m_pEffect->GetNumComponents() - 1);
				}
			}
			else
			{
				m_owner.m_pEffect->RemoveComponent(m_index, true);
			}
			m_owner.signalEdited(-1);
			m_owner.SetEffect(m_owner.m_pEffect);
			return true;
		}

		int    m_index, m_parentIndex;
		string m_name, m_asset;
	};
};

/////////////////////////////////////////////////////////////////////////
CEffectPanel::CEffectPanel(CEffectAssetModel* pModel, QWidget* pParent)
	: CDockableWidget(pParent)
	, m_pEffectAssetModel(pModel)
{
	auto pLayout = new QHBoxLayout();
	{
		auto pSplitter = new QSplitter(Qt::Horizontal);
		pSplitter->setChildrenCollapsible(false);
		pLayout->addWidget(pSplitter);

		pSplitter->addWidget(m_pComponentTree = new CComponentTree(this));
		pSplitter->addWidget(LabeledWidget(m_pFeatureList = new CFeatureList(this), "<b>Features</b>"));

		connect(m_pComponentTree, &QTreeWidget::itemClicked, this,
			[this](QTreeWidgetItem* item, int column)
		{
			IParticleComponent* pComp = m_pComponentTree->ComponentFromItem(item);
			m_pFeatureList->SetComponent(pComp);
			m_nComponent = m_pComponentTree->RowFromItem(item);
		});

		// Notify asset model of external edits
		m_pComponentTree->signalEdited.Connect([this](int nComp)
		{
			m_pEffectAssetModel->SetModified(true);
			m_pEffectAssetModel->GetEffect()->Update();
			m_pEffectAssetModel->signalEffectEdited(nComp, -1);
		});
		m_pFeatureList->signalEdited.Connect([this](int nFeature)
		{
			m_pEffectAssetModel->SetModified(true);
			m_pEffectAssetModel->GetEffect()->Update();
			m_pEffectAssetModel->signalEffectEdited(m_nComponent, nFeature);
		});
	}
	setLayout(pLayout);

	// Handle effect load events
	m_pEffectAssetModel->signalBeginEffectAssetChange.Connect(this, &CEffectPanel::onBeginEffectAssetChange);
	m_pEffectAssetModel->signalEndEffectAssetChange.Connect(this, &CEffectPanel::onEndEffectAssetChange);
	onEndEffectAssetChange();
}

CEffectPanel::~CEffectPanel()
{
	if (m_pEffectAssetModel)
	{
		m_pEffectAssetModel->signalBeginEffectAssetChange.DisconnectObject(this);
		m_pEffectAssetModel->signalEndEffectAssetChange.DisconnectObject(this);
	}
}

void CEffectPanel::onBeginEffectAssetChange()
{
	m_pComponentTree->clear();
	if (auto* graphModel = m_pEffectAssetModel->GetModel())
	{
		graphModel->disconnect(this);
	}
}

void CEffectPanel::onEndEffectAssetChange()
{
	if (auto pEffect = m_pEffectAssetModel->GetEffect())
	{
		SetEffect(pEffect);

		if (auto* graphModel = m_pEffectAssetModel->GetModel())
		{
			connect(graphModel, &CParticleGraphModel::SignalCreateNode,       this, &CEffectPanel::onEffectChanged);
			connect(graphModel, &CParticleGraphModel::SignalRemoveNode,       this, &CEffectPanel::onEffectChanged);
			connect(graphModel, &CParticleGraphModel::SignalCreateConnection, this, &CEffectPanel::onEffectChanged);
			connect(graphModel, &CParticleGraphModel::SignalRemoveConnection, this, &CEffectPanel::onEffectChanged);
			connect(graphModel, &CParticleGraphModel::SignalInvalidated,      this, &CEffectPanel::onEffectChanged);

			graphModel->signalChanged.Connect(this, &CEffectPanel::onComponentChanged);
		}
	}
}

void CEffectPanel::SetEffect(IParticleEffect* pEffect)
{
	m_pComponentTree->SetEffect(pEffect);
	m_pFeatureList->clear();
}

void CEffectPanel::onEffectChanged()
{
	m_updated = true;
}

void CEffectPanel::onComponentChanged(IParticleComponent* pComp)
{
	if (m_pEffectAssetModel)
	{
		if (auto* pEffect = m_pEffectAssetModel->GetEffect())
		{
			if (m_nComponent >= 0 && pEffect->GetComponent(m_nComponent) == pComp)
				m_pFeatureList->SetComponent(pComp);
		}
	}
}

void CEffectPanel::paintEvent(QPaintEvent* event)
{
	if (m_updated)
	{
		SetEffect(m_pEffectAssetModel->GetEffect());
		m_updated = false;
	}
	CDockableWidget::paintEvent(event);
}

}
