// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Serialization/PropertyTree/IDrawContext.h>
#include <Serialization/PropertyTree/PropertyRowField.h>
#include <Serialization/PropertyTree/PropertyTreeModel.h>
#include "Serialization/QPropertyTree/QPropertyTree.h"
#include "Serialization.h"
#include <CrySerialization/Decorators/OutputFilePath.h>
#include <CrySerialization/Decorators/OutputFilePathImpl.h>

using Serialization::OutputFilePath;

class PropertyRowOutputFilePath : public PropertyRowField
{
public:
	void                clear();

	bool                isLeaf() const override   { return true; }
	bool                isStatic() const override { return false; }

	bool                onActivate(const PropertyActivationEvent& e) override;
	void                setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                assignTo(const Serialization::SStruct& ser) const override;
	string              valueAsString() const;
	void                serializeValue(Serialization::IArchive& ar);
	bool                onContextMenu(IMenu& menu, PropertyTree* tree);
	const void*         searchHandle() const             { return handle_; }

	int                 buttonCount() const override     { return 1; }
	property_tree::Icon buttonIcon(const PropertyTree* tree, int index) const override;
	bool                usePathEllipsis() const override { return true; }

private:
	string      path_;
	string      filter_;
	string      startFolder_;
	const void* handle_;
};

struct OutputFilePathMenuHandler : PropertyRowMenuHandler
{
	PropertyTree*              tree;
	PropertyRowOutputFilePath* self;

	OutputFilePathMenuHandler(PropertyTree* tree, PropertyRowOutputFilePath* container);

	void onMenuClear();
};

