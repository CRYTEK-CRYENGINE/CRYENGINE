// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <CryCore/smartptr.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/TypeID.h>

namespace PropertyTree2
{
	class CRowModel;
}

//! In order to implement a property tree widget, create a derived QWidget and inherit from this interface. See PropertyTreeWidgets.h for examples.
//! Register the widget for your type using REGISTER_PROPERTY_WIDGET macro
struct IPropertyTreeWidget
{
	//!Serializes the content of this row, in order to save and restore its state, for instance for copy/pasting rows.
	//!If the internals have changed during serialize (paste operation), it may equate to changing the value, therefore OnChanged() needs to be called as a result.
	virtual void Serialize(Serialization::IArchive& ar) = 0;

	//!Sets the value of the widget from any of the viable types, may use contextual information from the archive
	virtual void SetValue(void* valuePtr, const yasli::TypeID& type, const yasli::Archive& ar) = 0;

	//!Gets the value from the widget into one of the viable types
	virtual void GetValue(void* valuePtr, const yasli::TypeID& type) const = 0;

	template<typename T>
	void SetValue(const T& valueRef, const yasli::Archive& ar) { SetValue((void*)&valueRef, yasli::TypeID::get<T>(), ar); }

	template<typename T>
	void GetValue(T& valueRef) const { GetValue(&valueRef, yasli::TypeID::get<T>()); }
	
	void SetValue(const yasli::Serializer& ser, const yasli::Archive& ar) {	SetValue(ser.pointer(), ser.type(), ar); }
	void GetValue(const yasli::Serializer& ser) const {	GetValue(ser.pointer(), ser.type()); }

	void SetValue(const yasli::Object& obj, const yasli::Archive& ar) { SetValue(obj.address(), obj.type(), ar); }
	void GetValue(const yasli::Object& obj) const { GetValue(obj.address(), obj.type()); }

	//! Returns true if the widget row is available for multi-edit. Must implement SetMultiEditValue() if returns true.
	virtual bool SupportsMultiEdit() const = 0;

	//!Called when we are displaying a field that represents different values from multiple objects
	virtual void SetMultiEditValue() { CRY_ASSERT(false); /* not implemented */ }

	//! If the widget's type is serializable and has children, returning true allows the property tree to show the child rows. By default they are not shown.
	virtual bool AllowChildSerialization() { return false; }

	//! Returns true if children can be reordered, this is true for dynamic containers
	virtual bool CanMoveChildren() const { return false; }

	virtual void MoveChild(int oldIndex, int newIndex) {}

	//! A widget has the possibility of adding context menu actions to a row's context menu.
	virtual void PopulateContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row) {}

	//! A widget has the possibility of adding context menu actions to a child's row's context menu.
	virtual void PopulateChildContextMenu(QMenu* menu, const PropertyTree2::CRowModel* row) {}

	//!Useful if the widget needs information from the parent row, such as the label.
	void SetRowModel(PropertyTree2::CRowModel* row) { m_rowModel = row; }

	//Change propagation. Widgets will call the methods, property trees attach to the signals.

	CCrySignal<void(const PropertyTree2::CRowModel*)> signalChanged;
	CCrySignal<void(const PropertyTree2::CRowModel*)> signalContinuousChanged;

	virtual ~IPropertyTreeWidget() {}

protected:

	//!Signals the property tree that the row's value has changed, triggers serialization and update
	void OnChanged() { signalChanged(m_rowModel); }

	//!Signals the property tree that the row's value has changed and will continue to change, triggers one way serialization to the object.
	//!This is useful for i.e. dragging operations. Once the change is complete OnChanged() should be called
	void OnContinuousChanged() { signalContinuousChanged(m_rowModel); }
	
	PropertyTree2::CRowModel* m_rowModel;
};

Q_DECLARE_INTERFACE(IPropertyTreeWidget, "PropertyTree2/IPropertyTreeWidget")

class CPropertyTreeWidgetFactory
{
public:
	typedef QWidget Product;

	struct FactoryBase
	{
		virtual Product* Create() = 0;
	};

	template<typename T>
	struct Factory : FactoryBase
	{
		Factory(const char* key, CPropertyTreeWidgetFactory& factory)
		{
			factory.Add(key, this);
		}

		virtual Product* Create() final { return new T(); }
	};

public:
	void Add(const char* name, FactoryBase* factory)
	{
		m_factories[name] = factory;
	}

	Product* Create(const char* typeName) const
	{
		auto it = m_factories.find(typeName);
		if (it != m_factories.end())
		{
			return it->second->Create();
		}

		return nullptr;
	}

	bool IsRegistered(const char* typeName) const
	{
		auto it = m_factories.find(typeName);
		return (it != m_factories.end());
	}

private:

	struct LessStrCmpPR
	{
		bool operator()(const char* a, const char* b) const {
			return strcmp(a, b) < 0;
		}
	};

	typedef std::map<const char*, FactoryBase*, LessStrCmpPR> Factories;
	Factories m_factories;
};

PROPERTY_TREE_API CPropertyTreeWidgetFactory& GetPropertyTreeWidgetFactory();

#define REGISTER_PROPERTY_WIDGET(DataType, WidgetType) \
	static CPropertyTreeWidgetFactory::Factory<WidgetType> g_CPropertyTreeWidgetFactory##DataType##Creator(yasli::TypeID::get<DataType>().name(), GetPropertyTreeWidgetFactory());

//Using a templated type as DataType will not compile with the above macro, use this one instead
#define REGISTER_PROPERTY_WIDGET_TEMPLATE(DataType, TemplateArg, WidgetType) \
	static CPropertyTreeWidgetFactory::Factory<WidgetType> g_CPropertyTreeWidgetFactory##TemplateArg##Creator(yasli::TypeID::get<DataType>().name(), GetPropertyTreeWidgetFactory());
