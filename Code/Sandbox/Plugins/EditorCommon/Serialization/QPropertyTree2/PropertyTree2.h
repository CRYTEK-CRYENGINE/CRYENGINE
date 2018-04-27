// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"

class QScrollArea;
class QSearchBox;

namespace PropertyTree2 
{
	class CRowModel;
	class CFormWidget;
	class CInlineWidgetBox;
}

//Replacement class for QPropertyTree. This is a much more straightforward implementation using QWidget components to display a property tree.
//Intended to be a similar interface to facilitate migration however not all features, signals and methods are supported.

//TODO: Possible improvements:
// * Overlay for drawing various highlights and effects: 
//		* See CFormRow limitations on the drag highlight
//		* Draw a highlight to indicate the current search row which may be different from the active row
// * templated attach method with SFINAE that wraps objects in Serialization::SStruct automatically or errors
class EDITOR_COMMON_API QPropertyTree2 : public QWidget
{
	Q_OBJECT

	Q_PROPERTY(QIcon collapsedIcon READ GetCollapsedIcon WRITE SetCollapsedIcon DESIGNABLE true)
	Q_PROPERTY(QIcon expandedIcon READ GetExpandedIcon WRITE SetExpandedIcon DESIGNABLE true)
	Q_PROPERTY(QIcon dragHandleIcon READ GetDragHandleIcon WRITE SetDragHandleIcon DESIGNABLE true)

	friend class PropertyTree2::CFormWidget;
	friend class PropertyTree2::CInlineWidgetBox;
public:
	explicit QPropertyTree2(QWidget* parent = nullptr);
	virtual ~QPropertyTree2();
	
	//! Attach and displays the property of an object. Does not take ownership of the object.
	void attach(const yasli::Serializer& serializer);

	//! Attach and displays the property of several objects. Does not take ownership.
	void attach(const yasli::Serializers& serializers);

	void attach(const yasli::Object& object);

	//! Detaches serializable objects and clears the tree
	void detach();

	//! Updates the tree by serializing the data from the object to the ui
	void revert();

	//! Updates the attached objects by serializing data from the ui to the objects
	void apply();

	//! Sets head of the context-list. Can be used to pass additional data to nested decorators.
	void setArchiveContext(yasli::Context* context) { m_archiveContext = context; }

	//! Automatically reverts after each change, guaranteeing that the tree is up to date if the serialization alters other properties or the general structure. Defaults to true.
	void setAutoRevert(bool autoRevert) { m_autoRevert = autoRevert; }

	//! Automatically expand rows up to specified depth (starts at 0)
	void setAutoExpandDepth(int depth) { m_autoExpandDepth = depth; }

	//! Legacy method
	void setExpandLevels(int depth) { setAutoExpandDepth(depth); }

signals:
	// Emitted for every finished changed of the value.  E.g. when you drag a slider,
	// signalChanged will be invoked when you release a mouse button.
	void signalChanged();
	// Used fast-pace changes, like movement of the slider before mouse gets released.
	void signalContinuousChange();

	// Called for each attached object before serialization
	void signalAboutToSerialize(const yasli::Serializer& object, yasli::Archive& ar);
	// Called for each attached object after serialization
	void signalSerialized(const yasli::Serializer& object, yasli::Archive& ar);
	
	// Invoked before any change is going to occur and can be used to store current version
	// of data for own undo stack.
	void signalPushUndo();

private:

	virtual void keyPressEvent(QKeyEvent* ev) override;
	virtual void customEvent(QEvent* event) override;

	void OnRowChanged(const PropertyTree2::CRowModel* row);
	void OnRowContinuousChanged(const PropertyTree2::CRowModel* row);
	void CopyRowValueToModels(const PropertyTree2::CRowModel* row);
	void OnFind();
	void OnFindPrevious();
	void OnFindNext();
	void OnCloseSearch();
	const PropertyTree2::CRowModel* DoFind(const PropertyTree2::CRowModel* currentRow, const QString& searchStr, bool searchUp = false);
	
	//! Computes the next row in vertical order, regardless of hierarchy
	const PropertyTree2::CRowModel* GetRowBelow(const PropertyTree2::CRowModel* row);
	//! Computes the previous row in vertical order, regardless of hierarchy
	const PropertyTree2::CRowModel* GetRowAbove(const PropertyTree2::CRowModel* row);
	//! "Selects" and scrolls to a row
	void FocusRow(const PropertyTree2::CRowModel* row);

	//! Will accumulate all the next changes until set to false again. Notifies once set to false if anything has changed.
	void SetAccumulateChanges(bool accumulate);

	bool IsDraggingSplitter() const { return m_isDraggingSplitter; }
	void SetDraggingSplitter(bool dragging) { m_isDraggingSplitter = dragging; }

	void SetSplitterPosition(int pos);
	int GetSplitterPosition() const;

	void SetActiveRow(const PropertyTree2::CRowModel* row);
	const PropertyTree2::CRowModel* GetActiveRow() { return m_activeRow.get(); }

	void EnsureRowVisible(const PropertyTree2::CRowModel* row);
	QScrollArea* GetScrollArea() { return m_scrollArea; }

	int GetAutoExpandDepth() const { return m_autoExpandDepth; }

	//Styling
	QIcon GetCollapsedIcon() const { return m_collapsedIcon; }
	void SetCollapsedIcon(QIcon icon) { m_collapsedIcon = icon; }

	QIcon GetExpandedIcon() const { return m_expandedIcon; }
	void SetExpandedIcon(QIcon icon) { m_expandedIcon = icon; }

	QIcon GetDragHandleIcon() const { return m_dragHandleIcon; }
	void SetDragHandleIcon(QIcon icon) { m_dragHandleIcon = icon; }


	typedef std::vector<yasli::Object> Objects;
	Objects m_attached;
	typedef std::vector<_smart_ptr<PropertyTree2::CRowModel>> Models;
	Models m_models;
	yasli::Context* m_archiveContext;
	_smart_ptr<PropertyTree2::CRowModel> m_root;

	QScrollArea* m_scrollArea;
	QSearchBox* m_searchBox;
	QWidget* m_searchWidget;
	PropertyTree2::CFormWidget* m_rootForm;
	_smart_ptr<const PropertyTree2::CRowModel> m_currentSearchRow;
	_smart_ptr<const PropertyTree2::CRowModel> m_activeRow;
	QString m_lastSearchText;

	float m_splitterRatio;
	bool m_isDraggingSplitter;

	bool m_autoRevert;
	bool m_ignoreChanges;
	int m_autoExpandDepth;

	enum class AccumulateChangesStatus
	{
		None,
		Accumulate,
		UndoPushed
	};
	AccumulateChangesStatus m_accumulateChanges;

	//Styling
	QIcon m_collapsedIcon;
	QIcon m_expandedIcon;
	QIcon m_dragHandleIcon;
};

