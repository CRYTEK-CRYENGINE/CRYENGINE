// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTreeLegacy/IDrawContext.h>
#include <Serialization/PropertyTreeLegacy/PropertyRowField.h>
#include <Serialization.h>
#include <IResourceSelectorHost.h>

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

	void                          jumpTo(PropertyTreeLegacy* tree);
	bool                          createFile(PropertyTreeLegacy* tree);
	void                          setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                          assignTo(const Serialization::SStruct& ser) const override;
	bool                          onShowToolTip() override;
	void                          onHideToolTip() override;
	bool                          onActivate(const PropertyActivationEvent& ev) override;
	bool                          onActivateButton(int button, const PropertyActivationEvent& e) override;
	bool                          getHoverInfo(PropertyHoverInfo* hover, const Point& cursorPos, const PropertyTreeLegacy* tree) const override;
	const void*                   searchHandle() const override { return searchHandle_; }
	Serialization::TypeID         typeId() const override       { return wrappedType_; }
	property_tree::InplaceWidget* createWidget(PropertyTreeLegacy* tree) override;
	void                          setValue(PropertyTreeLegacy* tree, const char* str, const void* handle, const yasli::TypeID& type);

	int                           buttonCount() const override;
	virtual property_tree::Icon   buttonIcon(const PropertyTreeLegacy* tree, int index) const override;
	virtual bool                  usePathEllipsis() const override { return true; }
	string                        valueAsString() const;
	void                          serializeValue(Serialization::IArchive& ar);
	void                          redraw(property_tree::IDrawContext& context);
	bool                          onMouseDown(PropertyTreeLegacy* tree, Point point, bool& changed) override;

	virtual bool                  onDataDragMove(PropertyTreeLegacy* tree) override;
	virtual bool                  onDataDrop(PropertyTreeLegacy* tree) override;

	bool                          onContextMenu(IMenu& menu, PropertyTreeLegacy* tree);
	bool                          pickResource(PropertyTreeLegacy* tree);
	bool                          editResource(PropertyTreeLegacy* tree);
	const char*                   typeNameForFilter(PropertyTreeLegacy* tree) const override { return !type_.empty() ? type_.c_str() : "ResourceSelector"; }
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
	bool                                bActive_; // A state to prevent re-entrance if the selector is already active.
	int id_;
};

struct ResourceSelectorMenuHandler : PropertyRowMenuHandler
{
	PropertyTreeLegacy*                tree;
	PropertyRowResourceSelector* self;

	ResourceSelectorMenuHandler(PropertyTreeLegacy* tree, PropertyRowResourceSelector* container);

	void onMenuCreateFile();
	void onMenuJumpTo();
	void onMenuClear();
	void onMenuPickResource();
};
