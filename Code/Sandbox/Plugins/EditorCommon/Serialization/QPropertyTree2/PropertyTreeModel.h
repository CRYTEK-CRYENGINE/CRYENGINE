// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/smartptr.h>
#include "IPropertyTreeWidget.h"

namespace PropertyTree2
{

//! The actual underlying model for the tree
//! Passed through archives and property trees, it creates and owns the widgets displayed, the PropertyTree only "borrows" them for laying out and displaying
class CRowModel : public _reference_target_t
{
public:

	//!Default constructor is intended for root rows
	CRowModel(CRowModel* parent);

	CRowModel(const char* name, const char* label, CRowModel* parent);

	//! Does not set the widget
	CRowModel(const char* name, const char* label, CRowModel* parent, const yasli::TypeID& type);

	~CRowModel();

	const string&  GetName() const { return m_name; }
	QString GetLabel() const { return m_label; }
	QString GetTooltip() const { return m_tooltip; }
	yasli::TypeID GetType() const { return m_type; }

	//! Creates the QWidget registered for this type
	template<typename DataType>
	void SetWidgetAndType()
	{
		SetWidgetAndType(yasli::TypeID::get<DataType>());
	}

	//! Creates the QWidget registered for this type
	void SetWidgetAndType(const yasli::TypeID& type);

	bool IsWidgetSet() const { return m_widget != nullptr; }

	template<typename DataType>
	void SetType()
	{
		SetType(yasli::TypeID::get<DataType>());
	}

	//Use for rows that do not have a widget
	void SetType(const yasli::TypeID& type);

	void SetLabel(const QString& label);

	const CRowModel* GetParent() const { return m_parent; }
	CRowModel* GetParent() { return m_parent; }

	//Note that the children are non-const here :(
	const std::vector<_smart_ptr<CRowModel>>& GetChildren() const { return m_children; }

	//! IsRoot if it is the root or if it has been detached from any other hierarchy.
	bool IsRoot() const { return m_parent == nullptr; }
	bool HasChildren() const { return !m_children.empty(); }

	//! Computes and returns the index of this row in the parent row, returns -1 if IsRoot()
	int GetIndex() const;

	QWidget* GetQWidget() const { return m_widget; }
	IPropertyTreeWidget* GetPropertyTreeWidget() const;

	void SetTooltip(const char* tooltip) { m_tooltip = tooltip; }

	// Flags and special behaviors:
	// Based on yasli's special characters at the beginning of the label. Only matches (^|!|+)* any other character will stop flag processing, only use the supported flags 
	enum Flag
	{
		Inline = 0x01, //The widget will be shown in the parent row instead. Only works if this is the first row of a nesting level, only one inlined widget is supported. (^)
		Hidden = 0x02, //The row is hidden
		ReadOnly = 0x04, //The widget will be disabled (!)
		ExpandByDefault = 0x08, //The row will be expanded by default (+)
		CollapseByDefault = 0x10, //The row will be collpsed by default (-)
		AllChildrenHidden = 0x20, //As a result of hidden flags, this row does not have visible children
	};

	typedef uint8 Flags;
	Flags GetFlags() const { return m_flags; }
	bool IsHidden() const { return (m_flags & Hidden) != 0; }
	bool HasVisibleChildren() const { return HasChildren() && !(m_flags & AllChildrenHidden); }

	//Dirty flag is used to minimize updates to the tree when changes happen
	void MarkClean() const;
	void MarkNotVisitedRecursive();
	void PruneNotVisitedChildren();

	bool IsClean() const;
	bool HasDirtyChildren() const;
	bool HasFirstTimeChildren() const;
	bool IsDirty() const;

	//Multi-edit features

	//! Merges with another model, leaving only the fields that exist in both and that are valid in a multi-edit context
	void Intersect(const CRowModel* other);

private:
	//This flag is used to catch updates to an already existing model.
	enum class DirtyFlag : uint8
	{
		NotVisited, //Not visited by PropertyOArchive, this row does not exist in the object and should be removed
		Clean, //Nothing has changed since last marked clean
		DirtyChild, //One of the children is dirty
		Dirty, //Has changed, or children have been added / removed
		DirtyFirstChildren, //Children have been added to this row, and it didn't have any before
	};

	void AddChild(CRowModel* child);
	void MarkDirty(DirtyFlag flag = DirtyFlag::Dirty);
	void UpdateFlags();

	string m_name;
	QString m_label;
	QString m_tooltip;
	yasli::TypeID m_type;

	//This class has the ownership of the widgets, they are detached from the property tree before being destructed
	QWidget* m_widget;

	CRowModel* m_parent;
	std::vector<_smart_ptr<CRowModel>> m_children;
	mutable DirtyFlag m_dirty;
	Flags m_flags;
	int m_index; //cached index in parent, in order to speed up calculations
};

}
