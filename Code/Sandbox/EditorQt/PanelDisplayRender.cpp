// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "PanelDisplayRender.h"
#include <Preferences/ViewportPreferences.h>
#include "Grid.h"

#include <QLayout>
#include <Serialization/QPropertyTree/QPropertyTree.h>

#include <CryRenderer/IStereoRenderer.h>

#include <Serialization.h>
#include <CrySerialization/Enum.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <CrySerialization/Decorators/Range.h>

#include <CryCore/SFunctor.h>

#define LEAVE_FLAGS (RENDER_FLAG_BBOX)

static CPanelDisplayRender* s_pPanelDisplayRender = 0;
static bool s_bNoRecurseUpdate = false;

enum ERenderMode
{
	eSolidMode     = R_SOLID_MODE,
	eWireframeMode = R_WIREFRAME_MODE,
};

enum ERenderStatsDebugMode
{
	eRenderStatsOff       = 0,
	eRenderStatsResources = 1,
	eRenderStatsDrawCalls = 6,
	eRenderStatsBudgets   = 3,
};

enum EDisplayStatsDebugMode
{
	eDisplayStatsOff     = 0,
	eDisplayStatsMinimal = 1,
	eDisplayStatsFull    = 2,
};

SERIALIZATION_ENUM_BEGIN(ERenderMode, LOCALIZE("Render Mode"))
SERIALIZATION_ENUM(eSolidMode, "solid", LOCALIZE("Solid"))
SERIALIZATION_ENUM(eWireframeMode, "wireframe", LOCALIZE("Wireframe"))
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(ERenderStatsDebugMode, "Display Stats")
SERIALIZATION_ENUM(eRenderStatsOff, "off", LOCALIZE("None"))
SERIALIZATION_ENUM(eRenderStatsResources, "resources", LOCALIZE("Render Resources"))
SERIALIZATION_ENUM(eRenderStatsDrawCalls, "drawcalls", LOCALIZE("Draw Calls per Object"))
SERIALIZATION_ENUM(eRenderStatsBudgets, "budgets", LOCALIZE("Render Budgets"))
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EDisplayStatsDebugMode, "Render Stats")
SERIALIZATION_ENUM(eDisplayStatsOff, "off", LOCALIZE("None"))
SERIALIZATION_ENUM(eDisplayStatsMinimal, "minimal", LOCALIZE("Minimal"))
SERIALIZATION_ENUM(eDisplayStatsFull, "full", LOCALIZE("Full"))
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EStereoMode, "Stereo Mode")
SERIALIZATION_ENUM(EStereoMode::STEREO_MODE_NO_STEREO, "off", LOCALIZE("No Stereo Output"))
SERIALIZATION_ENUM(EStereoMode::STEREO_MODE_DUAL_RENDERING, "dual", LOCALIZE("Dual Rendering"))
SERIALIZATION_ENUM(EStereoMode::STEREO_MODE_POST_STEREO, "post", LOCALIZE("Postprocess Stereo"))
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EStereoOutput, "Stereo Output")
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_STANDARD, "off", LOCALIZE("No Stereo Output (Emulation)"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_CHECKERBOARD, "Checkerboard", LOCALIZE("Checkerboard"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_ABOVE_AND_BELOW, "AboveAndBelow", LOCALIZE("Above and Below (not supported)"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_SIDE_BY_SIDE, "SideBySide", LOCALIZE("Side by Side"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_LINE_BY_LINE, "Interlaced", LOCALIZE("Interlaced (not supported)"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_ANAGLYPH, "Anaglyph", LOCALIZE("Anaglyph"))
SERIALIZATION_ENUM(EStereoOutput::STEREO_OUTPUT_HMD, "VR", LOCALIZE("VR Headset"))
SERIALIZATION_ENUM_END()

namespace Private_PanelDisplayRenderer
{
const char* render_category_cvars[] =
{
	"e_roads",                  LOCALIZE("Roads"),
	"e_Decals",                 LOCALIZE("Decals"),
	"e_DetailTextures",         LOCALIZE("Detail Texture"),
	"e_TerrainDetailMaterials", LOCALIZE("Terrain Detail Texture"),
	"e_Fog",                    LOCALIZE("Fog"),
	"e_Terrain",                LOCALIZE("Terrain"),
	"e_SkyBox",                 LOCALIZE("Sky Box"),
	"e_Shadows",                LOCALIZE("Shadows"),
	"e_Vegetation",             LOCALIZE("Vegetation"),
	"e_WaterOcean",             LOCALIZE("Ocean"),
	"e_WaterVolumes",           LOCALIZE("Water Volumes"),
	"e_ProcVegetation",         LOCALIZE("Procedural Vegetation"),
	"e_Particles",              LOCALIZE("Particles"),
	"e_Clouds",                 LOCALIZE("Clouds"),
	"e_GeomCaches",             LOCALIZE("Geom Caches"),
	"e_GI",                     LOCALIZE("Global Illumination"),
};

void OnCVarChanged(ICVar* var)
{
	if (s_pPanelDisplayRender && !s_bNoRecurseUpdate)
	{
		s_pPanelDisplayRender->OnCVarChangedCallback();
	}
}

void SerializeCVarBool(Serialization::IArchive& ar, const char* cvar, const char* display, int valueOn = 1, int valueOff = 0)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(cvar);
	if (pVar)
	{
		bool b = pVar->GetIVal() == valueOn;
		bool prev = b;
		ar(b, cvar, display);
		if (ar.isInput() && b != prev)
		{
			s_bNoRecurseUpdate = true;
			pVar->Set(b ? valueOn : valueOff);
			s_bNoRecurseUpdate = false;
		}
	}
}

void SerializeCVarFloat(Serialization::IArchive& ar, const char* cvar, const char* display, float limitMin, float limitMax)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(cvar);
	if (pVar)
	{
		float f = pVar->GetFVal();
		float prev = f;
		ar(Serialization::Range<float>(f, limitMin, limitMax), cvar, display);
		if (ar.isInput() && f != prev)
		{
			s_bNoRecurseUpdate = true;
			pVar->Set(f);
			s_bNoRecurseUpdate = false;
		}
	}
}

template<typename EnumType>
void SerializeCVarEnum(Serialization::IArchive& ar, const char* cvar, const char* display)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(cvar);
	if (pVar)
	{
		EnumType e = (EnumType)pVar->GetIVal();
		EnumType prev = e;
		ar(e, cvar, display);
		if (ar.isInput() && e != prev)
		{
			s_bNoRecurseUpdate = true;
			pVar->Set((int)e);
			s_bNoRecurseUpdate = false;
		}
	}
}

void SetCVarBool(const char* cvar, bool bEnable)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(cvar);
	if (pVar)
	{
		pVar->Set(bEnable ? 1 : 0);
	}
}
void InvertCVarBool(const char* cvar)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(cvar);
	if (pVar)
	{
		bool b = pVar->GetIVal() != 0;
		pVar->Set(b ? 0 : 1);
	}
}

void SerializeMaskValue(Serialization::IArchive& ar, uint32& value, uint32 mask, const char* name)
{
	bool b = (value & mask) == mask;
	ar(b, name, name);
	if (b)
		value |= mask;
	else
		value &= ~mask;
}
}

//////////////////////////////////////////////////////////////////////////
CPanelDisplayRender::CPanelDisplayRender(QWidget* parent, CViewport* viewport)
	: CDockableWidgetT<QScrollableBox>(parent)
	, m_pViewport(viewport)
{
	s_pPanelDisplayRender = this;

	SetupCallbacks();

	m_propertyTree = new QPropertyTree(this);

	m_propertyTree->setValueColumnWidth(0.6f);
	m_propertyTree->setAutoRevert(false);
	m_propertyTree->setAggregateMouseEvents(false);
	m_propertyTree->setFullRowContainers(true);
	m_propertyTree->setExpandLevels(3);
	m_propertyTree->setUndoEnabled(false);
	m_propertyTree->attach(Serialization::SStruct(*this));
	addWidget(m_propertyTree);
}

CPanelDisplayRender::~CPanelDisplayRender()
{
	RemoveCallbacks();
	s_pPanelDisplayRender = 0;
}

QSize CPanelDisplayRender::sizeHint() const
{
	return QSize(350, 600);
}

void CPanelDisplayRender::Serialize(Serialization::IArchive& ar)
{
	using namespace Private_PanelDisplayRenderer;

	if (m_pViewport)
	{
		m_pViewport->SerializeDisplayOptions(ar);
	}

	if (ar.openBlock("objtypes", "Hide by Category"))
	{
		if (ar.openBlock("buttons", "<"))
		{
			using Serialization::StdFunctionActionButton;
			ar(StdFunctionActionButton(std::bind(&CPanelDisplayRender::OnHideObjectsAll, this)), "all", "^All");
			ar(StdFunctionActionButton(std::bind(&CPanelDisplayRender::OnHideObjectsNone, this)), "none", "^None");
			ar(StdFunctionActionButton(std::bind(&CPanelDisplayRender::OnHideObjectsInvert, this)), "invert", "^Invert");
			ar.closeBlock();
		}

		uint32 m_mask = gViewportDebugPreferences.GetObjectHideMask();
		uint32 prevMask = m_mask;
		SerializeMaskValue(ar, m_mask, OBJTYPE_ENTITY, LOCALIZE("Entities"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_PREFAB, LOCALIZE("Prefabs"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_GROUP, LOCALIZE("Groups"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_TAGPOINT, LOCALIZE("Tag Points"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_AIPOINT, LOCALIZE("AI Points"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_SHAPE, LOCALIZE("Shapes/Paths"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_VOLUME, LOCALIZE("Volumes"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_BRUSH, LOCALIZE("Brushes"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_DECAL, LOCALIZE("Decals"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_SOLID, LOCALIZE("Solids"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_ROAD, LOCALIZE("Roads/Rivers"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_GEOMCACHE, LOCALIZE("GeomCaches"));
		SerializeMaskValue(ar, m_mask, OBJTYPE_OTHER, LOCALIZE("Other"));

		if (ar.isInput() && m_mask != prevMask)
		{
			SetObjectHideMask(m_mask);
		}
		ar.closeBlock();
	}

	if (ar.openBlock("categories", "Render Types"))
	{
		if (ar.openBlock("buttons", "<Enable"))
		{
			using Serialization::ActionButton;
			ar(ActionButton(std::bind(&CPanelDisplayRender::OnDisplayAll, this)), "all", "^All");
			ar(ActionButton(std::bind(&CPanelDisplayRender::OnDisplayNone, this)), "none", "^None");
			ar(ActionButton(std::bind(&CPanelDisplayRender::OnDisplayInvert, this)), "invert", "^Invert");
			ar.closeBlock();
		}

		for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
		{
			const char* cvar = render_category_cvars[i * 2];
			const char* display = render_category_cvars[i * 2 + 1];
			SerializeCVarBool(ar, cvar, display);
		}
		ar.closeBlock();
	}

	if (ar.openBlock("profile", "Display Options"))
	{
		SerializeCVarEnum<ERenderMode>(ar, "r_wireframe", "Render Mode");
		ar(gViewportPreferences.displayLabels, "Labels", "Labels");
		ar(gViewportPreferences.displayLinks, "Links", "Links");
		ar(gViewportPreferences.showSafeFrame, "Safe Frame", "Safe Frame");
		ar(gViewportPreferences.showTriggerBounds, "Trigger Bounds", "Trigger Bounds");
		ar(gViewportPreferences.showIcons, "Icons", "Icons");
		ar(gViewportPreferences.showSizeBasedIcons, "Size-based Icons", "Size-based Icons");

		ar(gViewportPreferences.showFrozenHelpers, "Frozen Object Helpers", "Frozen Object Helpers");

		ar.closeBlock();
	}

	if (ar.openBlock("profileblock", "Profile Options"))
	{
		SerializeCVarBool(ar, "Profile", LOCALIZE("Frame Profiler"));
		SerializeCVarBool(ar, "r_ProfileShaders", LOCALIZE("Profile Shaders"));
		//SerializeCVarBool( ar,"sys_enable_budgetmonitoring", LOCALIZE("Budgets") );
		SerializeCVarBool(ar, "r_MeasureOverdraw", LOCALIZE("Show Overdraw"));
		ar.closeBlock();
	}

	if (ar.openBlock("debug", "Debug Options"))
	{
		SerializeCVarBool(ar, "MemStats", LOCALIZE("Memory Info"), 1000, 0);
		//SerializeCVarBool( ar,"r_TexLog",                    LOCALIZE("") );
		SerializeCVarBool(ar, "p_draw_helpers", LOCALIZE("Show Physics Proxies"));
		SerializeCVarEnum<EDisplayStatsDebugMode>(ar, "r_displayInfo", "Display Debug");
		SerializeCVarEnum<ERenderStatsDebugMode>(ar, "r_Stats", "Render Debug");
		ar.closeBlock();
	}

	if (ar.openBlock("debug_ai", "AI/Game Options"))
	{
		SerializeCVarBool(ar, "ai_DebugDraw", LOCALIZE("AI Debug Info"), 1, -1);
		SerializeCVarBool(ar, "ai_DebugDrawCover", LOCALIZE("AI Draw Cover"), 2, 0);
		SerializeCVarBool(ar, "gt_show", LOCALIZE("Show Game Tokens"));
		ar.closeBlock();
	}

	if (ar.openBlock("debug_highlight", "Highlight"))
	{
		uint32 flags = gViewportDebugPreferences.GetDebugFlags();
		bool dbg_highlight_breakable = (flags & DBG_HIGHLIGHT_BREAKABLE) ? TRUE : FALSE;
		bool dbg_highlight_missing_surface_type = (flags & DBG_HIGHLIGHT_MISSING_SURFACE_TYPE) ? TRUE : FALSE;
		ar(dbg_highlight_breakable, "highlight_breakable_materials", LOCALIZE("Breakable Materials"));
		ar(dbg_highlight_missing_surface_type, "highlight_missing_sftypes", LOCALIZE("Missing Surface Types"));
		if (ar.isInput())
		{
			// turn both options off and reenable based on input
			flags &= ~(DBG_HIGHLIGHT_BREAKABLE | DBG_HIGHLIGHT_MISSING_SURFACE_TYPE);

			flags |= (dbg_highlight_breakable) ? DBG_HIGHLIGHT_BREAKABLE : 0;
			flags |= (dbg_highlight_missing_surface_type) ? DBG_HIGHLIGHT_MISSING_SURFACE_TYPE : 0;

			gViewportDebugPreferences.SetDebugFlags(flags);
		}
		ar.closeBlock();
	}

	SerializeStereo(ar);
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::OnHideObjectsAll()
{
	SetObjectHideMask(0xFFFFFFFF);
	m_propertyTree->revert();
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::OnHideObjectsNone()
{
	SetObjectHideMask(0);
	m_propertyTree->revert();
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::OnHideObjectsInvert()
{
	SetObjectHideMask(~gViewportDebugPreferences.GetObjectHideMask());
	m_propertyTree->revert();
}

void CPanelDisplayRender::SetupCallbacks()
{
	using namespace Private_PanelDisplayRenderer;

	for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
	{
		const char* cvar = render_category_cvars[i * 2];
		RegisterChangeCallback(cvar, &OnCVarChanged);
	}

	RegisterChangeCallback("r_wireframe", &OnCVarChanged);

	// Debug variables.
	RegisterChangeCallback("ai_DebugDraw", &OnCVarChanged);
	RegisterChangeCallback("ai_DebugDrawCover", &OnCVarChanged);
	RegisterChangeCallback("r_TexLog", &OnCVarChanged);
	RegisterChangeCallback("MemStats", &OnCVarChanged);
	RegisterChangeCallback("p_draw_helpers", &OnCVarChanged);
	RegisterChangeCallback("r_ProfileShaders", &OnCVarChanged);
	RegisterChangeCallback("r_MeasureOverdraw", &OnCVarChanged);
	RegisterChangeCallback("r_Stats", &OnCVarChanged);
	RegisterChangeCallback("sys_enable_budgetmonitoring", &OnCVarChanged);
	RegisterChangeCallback("gt_show", &OnCVarChanged);
	RegisterChangeCallback("Profile", &OnCVarChanged);

	// Stereo cvars
	RegisterChangeCallback("r_StereoMode", &OnCVarChanged);
	RegisterChangeCallback("r_StereoOutput", &OnCVarChanged);
	RegisterChangeCallback("r_StereoEyeDist", &OnCVarChanged);
	RegisterChangeCallback("r_StereoScreenDist", &OnCVarChanged);
	RegisterChangeCallback("r_StereoFlipEyes", &OnCVarChanged);
}

void CPanelDisplayRender::RemoveCallbacks()
{
	using namespace Private_PanelDisplayRenderer;

	for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
	{
		const char* cvar = render_category_cvars[i * 2];
		UnregisterChangeCallback(cvar);
	}

	UnregisterChangeCallback("r_wireframe");

	// Debug variables.
	UnregisterChangeCallback("ai_DebugDraw");
	UnregisterChangeCallback("ai_DebugDrawCover");
	UnregisterChangeCallback("r_TexLog");
	UnregisterChangeCallback("MemStats");
	UnregisterChangeCallback("p_draw_helpers");
	UnregisterChangeCallback("r_ProfileShaders");
	UnregisterChangeCallback("r_MeasureOverdraw");
	UnregisterChangeCallback("r_Stats");
	UnregisterChangeCallback("sys_enable_budgetmonitoring");
	UnregisterChangeCallback("gt_show");
	UnregisterChangeCallback("Profile");

	// Stereo cvars
	UnregisterChangeCallback("r_StereoMode");
	UnregisterChangeCallback("r_StereoOutput");
	UnregisterChangeCallback("r_StereoEyeDist");
	UnregisterChangeCallback("r_StereoScreenDist");
	UnregisterChangeCallback("r_StereoFlipEyes");
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::OnDisplayAll()
{
	using namespace Private_PanelDisplayRenderer;

	for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
	{
		const char* cvar = render_category_cvars[i * 2];
		SetCVarBool(cvar, 1);
	}
}

void CPanelDisplayRender::OnDisplayNone()
{
	using namespace Private_PanelDisplayRenderer;

	for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
	{
		const char* cvar = render_category_cvars[i * 2];
		SetCVarBool(cvar, 0);
	}
}

void CPanelDisplayRender::OnDisplayInvert()
{
	using namespace Private_PanelDisplayRenderer;

	for (int i = 0; i < _countof(render_category_cvars) / 2; i++)
	{
		const char* cvar = render_category_cvars[i * 2];
		InvertCVarBool(cvar);
	}
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::RegisterChangeCallback(const char* szVariableName, ConsoleVarFunc fnCallbackFunction)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(szVariableName);
	if (pVar)
	{
		if (m_varCallbackMap.find(pVar) == m_varCallbackMap.end())
		{
			SFunctor onChangeFunctor;
			onChangeFunctor.Set(fnCallbackFunction, pVar);
			uint64 index = pVar->GetNumberOfOnChangeFunctors();
			pVar->AddOnChangeFunctor(onChangeFunctor);
			m_varCallbackMap[pVar] = index;
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, "Attempting to register callback twice");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::UnregisterChangeCallback(const char* szVariableName)
{
	ICVar* pVar = gEnv->pConsole->GetCVar(szVariableName);
	if (pVar)
	{
		if (m_varCallbackMap.find(pVar) != m_varCallbackMap.end())
		{
			uint64 index = m_varCallbackMap[pVar];
			if (index != -1)
			{
				pVar->RemoveOnChangeFunctor(index);
				m_varCallbackMap[pVar] = -1;
			}
			else
			{
				CRY_ASSERT_MESSAGE(0, "Attempting to unregister callback twice");
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, "Attempting to unregister callback that doesn't exist");
		}
	}
}

void CPanelDisplayRender::OnCVarChangedCallback()
{
	m_propertyTree->revert();

	//TODO : review and fix later, is this the best place for this call?
	gEnv->p3DEngine->OnObjectModified(NULL, ERF_CASTSHADOWMAPS);
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnDisplayRenderUpdate:
		m_propertyTree->revert();
		break;
	}
}

void CPanelDisplayRender::SetObjectHideMask(uint32 mask)
{
	gViewportDebugPreferences.SetObjectHideMask(mask);
	GetIEditorImpl()->GetObjectManager()->InvalidateVisibleList();
	GetIEditorImpl()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
void CPanelDisplayRender::SerializeStereo(Serialization::IArchive& ar)
{
	using namespace Private_PanelDisplayRenderer;

	if (ar.openBlock("stereo", "Stereo Settings"))
	{
		SerializeCVarEnum<EStereoMode>(ar, "r_StereoMode", "Stereo Mode");
		SerializeCVarEnum<EStereoOutput>(ar, "r_StereoOutput", "Stereo Output");

		SerializeCVarFloat(ar, "r_StereoEyeDist", "Eye Distance (m)", -1, 1);
		SerializeCVarFloat(ar, "r_StereoScreenDist", "Screen Distance (m)", -1, 1);
		SerializeCVarBool(ar, "r_StereoFlipEyes", LOCALIZE("Flip Eyes"));
		ar.closeBlock();
	}
}

void CPanelDisplayRender::showEvent(QShowEvent* e)
{
	QWidget::showEvent(e);
	// Need to do that here, after widget is shown to make sure the contents are stretched to the correct dimensions.
	// QPropertyTree does take a size event before showing but for some reason the size it receives is not correct.
	// Intermediate widgets will have set the sizes state accordingly after they are shown, so
	// setting this here ensures that QPropertyTree::updateHeights is called.
	// If this is not done, then the last properties of the object are "eaten" by the layout until a new refresh/resize forces
	// QPropertyTree::updateHeights to be called.
	// Interestingly enough, there is no size event to QPropertyTree after the correct size is set, which leads me to believe that some
	// Qt bug might be in the works (or the weird hierarchy we use with QScrollArea confuses the whole system)
	if (m_propertyTree)
	{
		m_propertyTree->setSizeToContent(true);
	}
}

REGISTER_VIEWPANE_FACTORY_AND_MENU(CPanelDisplayRender, "Display Settings", "Viewport", false, "Viewport")

