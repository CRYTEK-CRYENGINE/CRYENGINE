// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/IDrawContext.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include <Serialization/PropertyTree/PropertyRowField.h>
#include <Serialization.h>
#include <CrySerialization/Decorators/Resources.h>
#include "IResourceSelectorHost.h"

using Serialization::IResourceSelector;
namespace Serialization {
struct INavigationProvider;
}

class PropertyRowResourceSelector : public PropertyRowField
{
public:
	PropertyRowResourceSelector();
	void                          clear();

	bool                          isLeaf() const override   { return true; }
	bool                          isStatic() const override { return false; }

	void                          jumpTo(PropertyTree* tree);
	bool                          createFile(PropertyTree* tree);
	void                          setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                          assignTo(const Serialization::SStruct& ser) const override;
	bool                          onShowToolTip() override;
	void                          onHideToolTip() override;
	bool                          onActivate(const PropertyActivationEvent& ev) override;
	bool                          onActivateButton(int button, const PropertyActivationEvent& e) override;
	bool                          getHoverInfo(PropertyHoverInfo* hover, const Point& cursorPos, const PropertyTree* tree) const override;
	const void*                   searchHandle() const override { return searchHandle_; }
	Serialization::TypeID         typeId() const override       { return wrappedType_; }
	property_tree::InplaceWidget* createWidget(PropertyTree* tree) override;
	void                          setValue(PropertyTree* tree, const char* str, const void* handle, const yasli::TypeID& type);

	int                           buttonCount() const override;
	virtual property_tree::Icon   buttonIcon(const PropertyTree* tree, int index) const override;
	virtual bool                  usePathEllipsis() const override { return true; }
	string                        valueAsString() const;
	void                          serializeValue(Serialization::IArchive& ar);
	void                          redraw(property_tree::IDrawContext& context);
	bool                          onMouseDown(PropertyTree* tree, Point point, bool& changed) override;

	virtual bool                  onDataDragMove(PropertyTree* tree) override;
	virtual bool                  onDataDrop(PropertyTree* tree) override;

	bool                          onContextMenu(IMenu& menu, PropertyTree* tree);
	bool                          pickResource(PropertyTree* tree);
	bool                          editResource(PropertyTree* tree);
	const char*                   typeNameForFilter(PropertyTree* tree) const override { return !type_.empty() ? type_.c_str() : "ResourceSelector"; }
	const yasli::string&          value() const                                        { return value_; }

private:
	mutable SResourceSelectorContext    context_;
	Serialization::INavigationProvider* provider_;
	const void*                         searchHandle_;
	Serialization::TypeID               wrappedType_;
	property_tree::Icon                 icon_;

	string                              value_;
	string                              type_;
	const SStaticResourceSelectorEntry*	selector_;
	string                              defaultPath_;
	bool                                bActive_; // A state to prevent re-entrancy if the selector is already active.
	int id_;
};

struct ResourceSelectorMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*                tree;
	PropertyRowResourceSelector* self;

	ResourceSelectorMenuHandler(PropertyTree* tree, PropertyRowResourceSelector* container);

	void onMenuCreateFile();
	void onMenuJumpTo();
	void onMenuClear();
	void onMenuPickResource();
};

