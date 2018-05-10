// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IDataBaseManager.h"
#include "UIs/PropertyTreePanel.h"
#include <CrySerialization/Enum.h>
#include "BoostPythonMacros.h"

class QWidget;

namespace Designer
{
class BaseTool;
class DesignerEditor;

enum EToolGroup
{
	eToolGroup_BasicSelection,
	eToolGroup_BasicSelectionCombination,
	eToolGroup_Selection,
	eToolGroup_Shape,
	eToolGroup_Edit,
	eToolGroup_Modify,
	eToolGroup_Surface,
	eToolGroup_Misc
};

enum EDesignerTool
{
	// BasicSelection Tools
	eDesigner_Vertex  = 0x0001,
	eDesigner_Edge    = 0x0002,
	eDesigner_Polygon = 0x0004,

	// BasicSelectionCombination Tools
	eDesigner_VertexEdge        = eDesigner_Vertex | eDesigner_Edge,
	eDesigner_VertexPolygon     = eDesigner_Vertex | eDesigner_Polygon,
	eDesigner_EdgePolygon       = eDesigner_Edge | eDesigner_Polygon,
	eDesigner_VertexEdgePolygon = eDesigner_Vertex | eDesigner_Edge | eDesigner_Polygon,

	// Rest BasicSelection Tools
	eDesigner_Pivot,

	// Selection Tools
	eDesigner_AllNone,
	eDesigner_Connected,
	eDesigner_Grow,
	eDesigner_Loop,
	eDesigner_Ring,
	eDesigner_Invert,

	// Shape Tools
	eDesigner_Box,
	eDesigner_Sphere,
	eDesigner_Cylinder,
	eDesigner_Cone,
	eDesigner_Rectangle,
	eDesigner_Disc,
	eDesigner_Polyline,
	eDesigner_Curve,
	eDesigner_Stair,
	eDesigner_StairProfile,
	eDesigner_CubeEditor,

	// Edit Tools
	eDesigner_Extrude,
	eDesigner_ExtrudeMultiple,
	eDesigner_ExtrudeEdge,
	eDesigner_Offset,
	eDesigner_Weld,
	eDesigner_Collapse,
	eDesigner_Fill,
	eDesigner_Flip,
	eDesigner_Merge,
	eDesigner_Separate,
	eDesigner_Remove,
	eDesigner_Copy,
	eDesigner_RemoveDoubles,

	// Modify Tools
	eDesigner_LoopCut,
	eDesigner_Bevel,
	eDesigner_Subdivision,
	eDesigner_Boolean,
	eDesigner_Slice,
	eDesigner_Mirror,
	eDesigner_Lathe,
	eDesigner_CircleClone,
	eDesigner_ArrayClone,

	// Surface Tools
	eDesigner_UVMapping,
	eDesigner_SmoothingGroup,

	// Misc Tools
	eDesigner_Pivot2Bottom,
	eDesigner_ResetXForm,
	eDesigner_Export,
	eDesigner_SnapToGrid,
	eDesigner_HidePolygon,
	eDesigner_Shortcut,

	// Invalid
	eDesigner_Invalid = 0x7FFFFFFF
};

class Polygon;

class IBasePanel
{
public:
	virtual void     Done()   {}
	virtual void     Update() {}
	virtual QWidget* GetWidget() = 0;
};

class IDesignerPanel : public IBasePanel
{
public:
	virtual void Init() = 0;
	virtual void DisableButton(EDesignerTool tool) = 0;
	virtual void SetButtonCheck(EDesignerTool tool, bool bChecked) = 0;
	virtual void OnDataBaseItemEvent(IDataBaseItem* pItem, EDataBaseItemEvent event) = 0;
	virtual void UpdateSubMaterialComboBox() = 0;
	virtual int  GetSubMatID() const = 0;
	virtual void SetSubMatID(int nID) = 0;
	virtual void UpdateCloneArrayButtons() = 0;
};

class IDesignerSubPanel : public IBasePanel
{
public:
	virtual void Init() = 0;
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) = 0;
	virtual void UpdateBackFaceCheckBox(Model* pModel) = 0;
};

template<class _Panel>
void DestroyPanel(_Panel** pPanel)
{
	if (*pPanel)
	{
		(*pPanel)->Done();
		*pPanel = NULL;
	}
}

template<class _Key, class _Base, class _Arg0, class _KeyPred = std::less<_Key>>
class DesignerFactory
{
public:

	struct ICreator
	{
		virtual ~ICreator() {}
		virtual _Base* Create(_Key key, _Arg0 tool) = 0;
	};

	template<class _DerivedPanel, class _Constructor>
	struct GeneralPanelCreator : public ICreator
	{
		GeneralPanelCreator(_Key key)
		{
			DesignerFactory::the().Add(key, this);
		}
		_Base* Create(_Key key, _Arg0 arg0) override
		{
			return _Constructor::Create();
		}
	};

	template<class _DerivedTool, class _DerivedPanel>
	struct AttributePanelCreator : public ICreator
	{
		AttributePanelCreator(_Key key)
		{
			DesignerFactory::the().Add(key, this);
		}

		_Base* Create(_Key key, _Arg0 tool) override
		{
			return new _DerivedPanel((_DerivedTool*)tool);
		}
	};

	template<class _DerivedTool>
	struct ToolCreator : public ICreator
	{
		ToolCreator(_Key key)
		{
			DesignerFactory::the().Add(key, this);
		}

		_Base* Create(_Key key, _Arg0 arg0) override
		{
			return new _DerivedTool(key);
		}
	};

	typedef std::map<_Key, ICreator*, _KeyPred> Creators;

	void Add(_Key& key, ICreator* const creator)
	{
		creators[key] = creator;
	}

	_Base* Create(_Key key, _Arg0 arg0 = NULL)
	{
		auto ii = creators.find(key);
		if (ii != creators.end())
			return ii->second->Create(ii->first, arg0);
		return NULL;
	}

	static DesignerFactory& the()
	{
		static DesignerFactory factory;
		return factory;
	}

private:
	DesignerFactory() {}
	Creators creators;
};

typedef std::map<EDesignerTool, _smart_ptr<BaseTool>> TOOLDESIGNER_MAP;
typedef std::map<EToolGroup, std::set<EDesignerTool>> TOOLGROUP_MAP;

class ToolGroupMapper
{
public:
	static ToolGroupMapper& the()
	{
		static ToolGroupMapper toolGroupMapper;
		return toolGroupMapper;
	}

	void AddTool(EToolGroup group, EDesignerTool tool)
	{
		m_ToolGroupMap[group].insert(tool);
	}

	std::vector<EDesignerTool> GetToolList(EToolGroup group)
	{
		auto iter = m_ToolGroupMap.find(group);
		std::vector<EDesignerTool> toolList;
		toolList.insert(toolList.begin(), iter->second.begin(), iter->second.end());
		return toolList;
	}

private:

	ToolGroupMapper(){}
	TOOLGROUP_MAP m_ToolGroupMap;
};

class ToolGroupRegister
{
public:
	ToolGroupRegister(EToolGroup group, EDesignerTool tool, const char* name, const char* classname)
	{
		ToolGroupMapper::the().AddTool(group, tool);
		Serialization::getEnumDescription<EDesignerTool>().add(tool, name, classname);
	}
};

struct LessStrCmpPR
{
	bool operator()(const char* a, const char* b) const
	{
		return strcmp(a, b) < 0;
	}
};

typedef DesignerFactory<const char*, IBasePanel, BaseTool*, LessStrCmpPR> UIFactory;
typedef DesignerFactory<EDesignerTool, IBasePanel, BaseTool*>             UIPanelFactory;
typedef DesignerFactory<EDesignerTool, BaseTool, int>                     ToolFactory;
}

#define REGISTER_GENERALPANEL(key, derived_panel, constructor) \
  static Designer::UIFactory::GeneralPanelCreator<Designer::derived_panel, Designer::constructor> generalFactory ## derived_panel ## key( # key);

#define PYTHON_DESIGNER_COMMAND(_key, _class)               \
	namespace DesignerCommand                                 \
	{                                                         \
	static void Py ## _class()                                \
	{                                                         \
	if (Designer::GetDesigner())                            \
	    Designer::GetDesigner()->SwitchTool(Designer::_key);  \
	}                                                         \
	}

#define REGISTER_DESIGNER_TOOL(_key, _group, _name, _class)                                          \
  static Designer::ToolFactory::ToolCreator<Designer::_class> toolFactory ## _class(Designer::_key); \
  static Designer::ToolGroupRegister toolGroupRegister ## _class(Designer::_group, Designer::_key, _name, # _class);

#define REGISTER_DESIGNER_TOOL_AND_COMMAND(_key, _group, _name, _class, _cmd_functionName, _cmd_description, _cmd_example) \
	PYTHON_DESIGNER_COMMAND(_key, _class)                                                                                    \
	REGISTER_DESIGNER_TOOL(_key, _group, _name, _class)                                                                      \
	REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerCommand::Py ## _class, designer, _cmd_functionName, _cmd_description, _cmd_example)

#define REGISTER_DESIGNER_TOOL_WITH_PANEL(_key, _group, _name, _class, _panel) \
  REGISTER_DESIGNER_TOOL(_key, _group, _name, _class)                          \
  static Designer::UIPanelFactory::AttributePanelCreator<Designer::_class, Designer::_panel> attributeUIFactory ## _class(Designer::_key);

#define REGISTER_DESIGNER_TOOL_WITH_PANEL_AND_COMMAND(_key, _group, _name, _class, _panel, _cmd_functionName, _cmd_description, _cmd_example) \
	PYTHON_DESIGNER_COMMAND(_key, _class)                                                                                                       \
	REGISTER_DESIGNER_TOOL_WITH_PANEL(_key, _group, _name, _class, _panel)                                                                      \
	REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerCommand::Py ## _class, designer, _cmd_functionName, _cmd_description, _cmd_example)

#define REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL(_key, _group, _name, _class) \
  REGISTER_DESIGNER_TOOL_WITH_PANEL(_key, _group, _name, _class, PropertyTreePanel<Designer::_class> )

#define REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(_key, _group, _name, _class, _cmd_functionName, _cmd_description, _cmd_example) \
	PYTHON_DESIGNER_COMMAND(_key, _class)                                                                                                            \
	REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL(_key, _group, _name, _class)                                                                      \
	REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(DesignerCommand::Py ## _class, designer, _cmd_functionName, _cmd_description, _cmd_example)

#define GENERATE_MOVETOOL_CLASS(type, ToolEnum)            \
  class Move ## type ## Tool : public MoveTool             \
  {                                                        \
  public:                                                  \
    Move ## type ## Tool(ToolEnum tool) : MoveTool(tool){} \
  };

