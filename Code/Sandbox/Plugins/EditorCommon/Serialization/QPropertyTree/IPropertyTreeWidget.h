// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include "RecursionLoopGuard.h"

#include <CryCore/smartptr.h>
#include <CrySerialization/yasli/Serializer.h>
#include <CrySerialization/yasli/TypeID.h>
#include <CrySandbox/CrySignal.h>
#include <QObject>
#include <stack>

class QMenu;

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
	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) = 0;

	//!Gets the value from the widget into one of the viable types
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const = 0;

	template<typename T>
	void SetValue(const T& valueRef, const yasli::Archive& ar) { SetValue((void*)&valueRef, yasli::TypeID::get<T>(), ar); }

	template<typename T>
	void GetValue(T& valueRef) const                                      { GetValue(&valueRef, yasli::TypeID::get<T>()); }

	void SetValue(const yasli::Serializer& ser, const yasli::Archive& ar) { SetValue(ser.pointer(), ser.type(), ar); }
	void GetValue(const yasli::Serializer& ser) const                     { GetValue(ser.pointer(), ser.type()); }

	void SetValue(const yasli::Object& obj, const yasli::Archive& ar)     { SetValue(obj.address(), obj.type(), ar); }
	void GetValue(const yasli::Object& obj) const                         { GetValue(obj.address(), obj.type()); }

	//! Returns true if the widget row is available for multi-edit. Must implement SetMultiEditValue() if returns true.
	virtual bool SupportsMultiEdit() const = 0;

	//!Called when we are displaying a field that represents different values from multiple objects
	virtual void SetMultiEditValue() { CRY_ASSERT(false); /* not implemented */ }

	//! If the widget's type is serializable and has children, returning true allows the property tree to show the child rows. By default they are not shown.
	virtual bool AllowChildSerialization() { return false; }

	//! Returns true if children can be reordered, this is true for dynamic containers
	virtual bool CanMoveChildren() const               { return false; }

	virtual void MoveChild(int oldIndex, int newIndex) {}

	//! A widget has the possibility of adding context menu actions to a row's context menu.
	virtual void PopulateContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow) {}

	//! A widget has the possibility of adding context menu actions to a child's row's context menu.
	virtual void PopulateChildContextMenu(QMenu* pMenu, const PropertyTree2::CRowModel* pRow) {}

	//!Useful if the widget needs information from the parent row, such as the label.
	void SetRowModel(PropertyTree2::CRowModel* pRow) { m_rowModel = pRow; }

	//!Change propagation. Widgets will call these methods, property trees attach to these signals.
	//!Usually Property Trees will use these signals to register undos, and undo object is registered on PreChanged and accepted on Change
	//!Flow is signalPreChanged -> signalContinuousChanged (might or might not be called, if called it can happen multiple times) -> signalChanged/signalDiscarded

	//!Called when the "final" change of a value happens, these can either be called by itself (like when you change a bool widget) or
	//!after a set of continuous changes (like when you scroll a float value widget)
	CCrySignal<void(const PropertyTree2::CRowModel*)> signalChanged;
	//!Called when a change operation is discarded by the user (like canceling a resource picker)
	CCrySignal<void(const PropertyTree2::CRowModel*)> signalDiscarded;
	//!Called when a continuous change of a value happens, there can be multiple continuous changes. A set of continuous changes is followed by a final non continuous change
	CCrySignal<void(const PropertyTree2::CRowModel*)> signalContinuousChanged;
	//!Called before any kind of change be it continuous or complete happens
	CCrySignal<void(const PropertyTree2::CRowModel*)> signalPreChanged;

	virtual ~IPropertyTreeWidget() {}

protected:

	//!Signals the property tree that the row's value has changed, triggers serialization and update
	void OnChanged()
	{
		if (m_ignoreChangeSignals)
		{
			return;
		}

		if (m_isContinuousChange)
		{
			//this is when a continuous change is confirmed, this mean that the pre event was already called
			signalChanged(m_rowModel);
		}
		else
		{
			//this is the first change, we need to send the pre event so that undo can be recorded
			signalPreChanged(m_rowModel);
			signalChanged(m_rowModel);
		}

		m_isContinuousChange = false;
	}

	//!Signals the property tree that the row's value change has been discarded, triggers serialization and update
	void OnDiscarded()
	{
		if (m_ignoreChangeSignals)
		{
			return;
		}

		//this is when a continuous change is cancelled
		signalDiscarded(m_rowModel);
		m_isContinuousChange = false;
	}

	//!Signals the property tree that the row's value has changed and will continue to change, triggers one way serialization to the object.
	//!This is useful for i.e. dragging operations. Once the change is complete OnChanged() should be called
	void OnContinuousChanged()
	{
		if (m_ignoreChangeSignals)
		{
			return;
		}

		if (m_isContinuousChange)
		{
			signalContinuousChanged(m_rowModel);
		}
		else
		{
			//this is the first change, we need to send the pre event so that undo can be recorded
			signalPreChanged(m_rowModel);
			signalContinuousChanged(m_rowModel);
			m_isContinuousChange = true;
		}
	}

	//!If we are in the middle of a continuous change, aka signalPreChanged was called but no signalChanged/signalDiscarded
	bool IsContinuousChange() { return m_isContinuousChange; };

	PropertyTree2::CRowModel* m_rowModel;

protected:
	//!Ignore change signals received by the widget
	bool m_ignoreChangeSignals = false;

private:
	bool m_isContinuousChange = false;
};

Q_DECLARE_INTERFACE(IPropertyTreeWidget, "PropertyTree2/IPropertyTreeWidget")

class CPropertyTreeWidgetFactory
{
public:
	typedef QWidget     Product;
	typedef const char* TypeID;

	struct FactoryBase
	{
		virtual Product* Create() = 0;

		virtual void     Release(Product* pProduct) = 0;
	};

	template<typename T>
	struct Factory : FactoryBase
	{
		Factory(TypeID id, CPropertyTreeWidgetFactory& factory)
		{
			factory.Add(id, this);
		}

		virtual Product* Create() final
		{
			if (!m_productPool.empty())
			{
				Product* pProduct = m_productPool.top();
				m_productPool.pop();
				return pProduct;
			}

			return new T();
		}

		virtual void Release(Product* pProduct) override
		{
			pProduct->setParent(nullptr);
			m_productPool.push(pProduct);
		}

	private:
		std::stack<Product*> m_productPool;
	};

public:
	void Add(TypeID id, FactoryBase* factory)
	{
		m_factories[id] = factory;
	}

	FactoryBase* GetFactory(TypeID id)
	{
		auto it = m_factories.find(id);
		if (it != m_factories.end())
		{
			return it->second;
		}

		return nullptr;
	}

	bool IsRegistered(TypeID id) const
	{
		auto it = m_factories.find(id);
		return (it != m_factories.end());
	}

private:

	struct LessStrCmpPR
	{
		bool operator()(const char* a, const char* b) const
		{
			return strcmp(a, b) < 0;
		}
	};

	typedef std::map<TypeID, FactoryBase*, LessStrCmpPR> Factories;
	Factories m_factories;
};

PROPERTY_TREE_API CPropertyTreeWidgetFactory& GetPropertyTreeWidgetFactory();

#define REGISTER_PROPERTY_WIDGET(DataType, WidgetType) \
	static CPropertyTreeWidgetFactory::Factory<WidgetType> g_CPropertyTreeWidgetFactory ## DataType ## Creator(yasli::TypeID::get<DataType>().name(), GetPropertyTreeWidgetFactory());

//Using a templated type as DataType will not compile with the above macro, use this one instead
#define REGISTER_PROPERTY_WIDGET_TEMPLATE(DataType, TemplateArg, WidgetType) \
	static CPropertyTreeWidgetFactory::Factory<WidgetType> g_CPropertyTreeWidgetFactory ## TemplateArg ## Creator(yasli::TypeID::get<DataType>().name(), GetPropertyTreeWidgetFactory());
