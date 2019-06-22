// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

// EditorCommon
#include "EditorCommonAPI.h"

// CryQt
#include "CryIcon.h"

// CryCommon
#include <CryCore/smartptr.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/Object.h>
#include <CrySerialization/yasli/Archive.h>

// Qt
#include <QWidget>

class QScrollArea;
class QSearchBox;
class QWheelEvent;

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
class EDITOR_COMMON_API QPropertyTree : public QWidget
{
	Q_OBJECT

	Q_PROPERTY(QIcon collapsedIcon READ GetCollapsedIcon WRITE SetCollapsedIcon DESIGNABLE true)
	Q_PROPERTY(QIcon expandedIcon READ GetExpandedIcon WRITE SetExpandedIcon DESIGNABLE true)
	Q_PROPERTY(QIcon dragHandleIcon READ GetDragHandleIcon WRITE SetDragHandleIcon DESIGNABLE true)

	friend class PropertyTree2::CFormWidget;
	friend class PropertyTree2::CInlineWidgetBox;
public:
	explicit QPropertyTree(QWidget* pParent = nullptr);
	virtual ~QPropertyTree();

	//! Attach and displays the property of an object. Does not take ownership of the object.
	void attach(const yasli::Serializer& serializer);

	//! Attach and displays the property of several objects. Does not take ownership.
	void attach(const yasli::Serializers& serializers);

	void attach(const yasli::Object& object);

	//! Detaches serializable objects and clears the tree
	void detach();

	//! Updates the tree by serializing the data from the object to the ui. Object -> UI
	void revert();

	//! Updates the attached objects by serializing data from the ui to the objects. UI -> Object
	void apply();

	//! Sets head of the context-list. Can be used to pass additional data to nested decorators.
	void setArchiveContext(yasli::Context* pContext) { m_pArchiveContext = pContext; }

	//! Automatically reverts after each change, guaranteeing that the tree is up to date if the serialization alters other properties or the general structure. Defaults to true.
	void setAutoRevert(bool autoRevert) { m_autoRevert = autoRevert; }

	//! Automatically expand rows up to specified depth (starts at 0)
	void setAutoExpandDepth(int depth) { m_autoExpandDepth = depth; }

	//! Legacy method
	void setExpandLevels(int depth) { setAutoExpandDepth(depth); }

signals:
	// Invoked before any change is going to occur and can be used to store current version
	// of data for undo stack.
	void signalPreChanged();
	// Emitted for every finished changed of the value.  E.g. when you drag a slider,
	// signalChanged will be invoked when you release a mouse button.
	void signalChanged();
	// Emitted for every change of a value that was discarded by the user
	void signalDiscarded();
	// Used fast-pace changes, like movement of the slider before mouse gets released.
	void signalContinuousChange();

	// Called for each attached object before serialization
	void signalAboutToSerialize(const yasli::Serializer& object, yasli::Archive& ar);
	// Called for each attached object after serialization
	void signalSerialized(const yasli::Serializer& object, yasli::Archive& ar);

private:
	class CScrollArea;

	virtual void keyPressEvent(QKeyEvent* pEvent) override;
	virtual void customEvent(QEvent* pEvent) override;

	//!Used to register undos on widget change, we record the undo on PreChange and accept/discard it on Changed/Discarded
	void                            OnRowChanged(const PropertyTree2::CRowModel* pRow);
	void                            OnRowDiscarded(const PropertyTree2::CRowModel* pRow);
	void                            OnRowContinuousChanged(const PropertyTree2::CRowModel* pRow);
	void                            OnRowPreChanged(const PropertyTree2::CRowModel* pRow);

	void                            CopyRowValueToModels(const PropertyTree2::CRowModel* pRow);
	void                            OnFind();
	void                            OnFindPrevious();
	void                            OnFindNext();
	void                            OnCloseSearch();
	const PropertyTree2::CRowModel* DoFind(const PropertyTree2::CRowModel* pCurrentRow, const QString& searchString, bool searchUp = false);

	//! Computes the next row in vertical order, regardless of hierarchy
	const PropertyTree2::CRowModel* GetRowBelow(const PropertyTree2::CRowModel* pRow);
	//! Computes the previous row in vertical order, regardless of hierarchy
	const PropertyTree2::CRowModel* GetRowAbove(const PropertyTree2::CRowModel* pRow);
	//! "Selects" and scrolls to a row
	void                            FocusRow(const PropertyTree2::CRowModel* pRow);

	//! Will accumulate all the next changes until set to false again. Notifies once set to false if anything has changed.
	void                            SetAccumulateChanges(bool accumulate);

	bool                            IsDraggingSplitter() const         { return m_isDraggingSplitter; }
	void                            SetDraggingSplitter(bool dragging) { m_isDraggingSplitter = dragging; }

	void                            SetSplitterPosition(int position);
	int                             GetSplitterPosition() const;

	void                            SetActiveRow(const PropertyTree2::CRowModel* pRow);
	const PropertyTree2::CRowModel* GetActiveRow() { return m_pActiveRow.get(); }

	void                            EnsureRowVisible(const PropertyTree2::CRowModel* pRow);
	QScrollArea*                    GetScrollArea();
	//! Used to intercept widget wheel events and forward to the scroll area
	void                            HandleScroll(QWheelEvent* pEvent);

	int                             GetAutoExpandDepth() const { return m_autoExpandDepth; }

	//Styling
	QIcon GetCollapsedIcon() const      { return m_collapsedIcon; }
	void  SetCollapsedIcon(QIcon icon)  { m_collapsedIcon = icon; }

	QIcon GetExpandedIcon() const       { return m_expandedIcon; }
	void  SetExpandedIcon(QIcon icon)   { m_expandedIcon = icon; }

	QIcon GetDragHandleIcon() const     { return m_dragHandleIcon; }
	void  SetDragHandleIcon(QIcon icon) { m_dragHandleIcon = icon; }

	typedef std::vector<yasli::Object>                        Objects;
	Objects                                    m_attached;
	typedef std::vector<_smart_ptr<PropertyTree2::CRowModel>> Models;
	Models                                     m_models;
	yasli::Context*                            m_pArchiveContext;
	_smart_ptr<PropertyTree2::CRowModel>       m_pRoot;

	CScrollArea*                               m_pScrollArea;
	QSearchBox*                                m_pSearchBox;
	QWidget*                                   m_pSearchWidget;
	PropertyTree2::CFormWidget*                m_pRootForm;
	_smart_ptr<const PropertyTree2::CRowModel> m_pCurrentSearchRow;
	_smart_ptr<const PropertyTree2::CRowModel> m_pActiveRow;
	QString                                    m_lastSearchText;

	float m_splitterRatio;
	bool  m_isDraggingSplitter;

	bool  m_autoRevert;
	bool  m_ignoreChanges;
	int   m_autoExpandDepth;

	enum class AccumulateChangesStatus
	{
		None,
		Accumulate,
		UndoPushed
	};
	AccumulateChangesStatus m_accumulateChanges;

	//Styling
	CryIcon m_collapsedIcon;
	CryIcon m_expandedIcon;
	CryIcon m_dragHandleIcon;
};
