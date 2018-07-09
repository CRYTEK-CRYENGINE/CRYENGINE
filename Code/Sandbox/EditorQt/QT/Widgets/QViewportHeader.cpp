// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "QViewportHeader.h"

#include "Viewport.h"
#include "RenderViewport.h"
#include "EditMode/ObjectMode.h"

#include "ViewManager.h"
#include <Preferences/ViewportPreferences.h>

#include "Dialogs/ToolbarDialog.h"
#include "CustomResolutionDlg.h"
#include "Controls/DynamicPopupMenu.h"
#include "Controls/QuestionDialog.h"
#include "Objects/ObjectLayerManager.h"
#include "Commands/QPythonAction.h"
#include "QT/QToolTabManager.h"
#include "QControls.h"

#include <CryCore/SFunctor.h>

#include <QToolButton>
#include <QPushButton>
#include <QWidget>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QAction>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QWidgetAction>
#include <QDialog>
#include <QTimer>
#include <QGraphicsDropShadowEffect>

#include "QT/QtMainFrame.h"
#include "Qt/QtMainFrameWidgets.h"
#include "AlignTool.h"
#include "PickObjectTool.h"
#include "EditMode/VertexSnappingModeTool.h"
#include "PanelDisplayRender.h"
#include "CryEdit.h"
#include "Dialogs/QNumericBoxDialog.h"

#include <CryIcon.h>
#include <EditorStyleHelper.h>
#include "LevelEditor/LevelEditorViewport.h"

static QColor Interpolate(const QColor& a, const QColor& b, float k)
{
	float mk = 1.0f - k;
	return QColor(a.red() * mk + b.red() * k,
	              a.green() * mk + b.green() * k,
	              a.blue() * mk + b.blue() * k,
	              a.alpha() * mk + b.alpha() * k);
}

class QViewportHeaderText : public QLabel
{
public:
	QViewportHeaderText(QWidget* parent)
		: QLabel(parent)
	{
		QFont font;
		font.setBold(true);
		setFont(font);
	}

	void paintEvent(QPaintEvent* ev) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing, true);
		QRect r = rect().adjusted(2, 2, -3, -3);
		p.translate(0.5f, 0.5f);
		p.setBrush(QBrush(GetStyleHelper()->viewportHeaderBackground()));
		p.setPen(Qt::NoPen);
		p.drawRoundedRect(r, 4, 4, Qt::AbsoluteSize);
		p.setPen(QPen(GetStyleHelper()->windowTextColor()));
		QTextOption textOption(Qt::AlignLeft | Qt::AlignVCenter);
		textOption.setWrapMode(QTextOption::NoWrap);
		p.drawText(r.adjusted(4, 0, 0, 0), text(), textOption);
	}
};

class CameraSpeedPopup : public QWidget
{
public:
	CameraSpeedPopup(CRenderViewport* pViewport, QWidget* pParent = nullptr)
		: QWidget(nullptr, Qt::SubWindow | Qt::FramelessWindowHint)
		, m_parent(pParent)
	{
		setAttribute(Qt::WA_ShowWithoutActivating);
		setAttribute(Qt::WA_TranslucentBackground);
		setObjectName("cameraSpeedViewportPopup");
		setAccessibleName("cameraSpeedViewportPopup");

		QBoxLayout* blayout = new QHBoxLayout();
		m_speedctrl = new QPrecisionSlider(Qt::Horizontal, this);

		setLayout(blayout);

		// we have 9999 steps from 0.01 to 100.0
		int numsteps = log(9999) / log(1.1);

		m_speedctrl->setMinimum(0);
		m_speedctrl->setMaximum(numsteps);
		//m_speedctrl->setTickPosition(QSlider::TicksBelow);
		m_speedctrl->setTickInterval(numsteps / 20);
		m_speedctrl->setAttribute(Qt::WA_TranslucentBackground);
		m_speedctrl->setObjectName("cameraSpeedPopupSlider");
		m_speedctrl->setAccessibleName("cameraSpeedPopupSlider");

		m_speedctrl->setValue(pViewport->GetCameraMoveSpeedIncrements());

		QObject::connect(m_speedctrl, &QPrecisionSlider::OnSliderPress, [=](int value)
		{
			m_timer->stop();
		});
		QObject::connect(m_speedctrl, &QPrecisionSlider::OnSliderRelease, [=](int value)
		{
			m_timer->start(s_timerValue);
		});
		QObject::connect(m_speedctrl, &QPrecisionSlider::OnSliderChanged, [=](int value)
		{
			pViewport->SetCameraMoveSpeedIncrements(value);
			m_timer->start(s_timerValue);
		});

		QLabel* speedLabel = new QLabel("Speed");
		speedLabel->setAttribute(Qt::WA_TranslucentBackground);
		// use shadow for the text so it's readable in any background
		QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect(speedLabel);
		effect->setBlurRadius(0);
		effect->setColor(QColor(Qt::black));
		effect->setOffset(1, 1);
		speedLabel->setGraphicsEffect(effect);

		blayout->addWidget(speedLabel);
		blayout->addWidget(m_speedctrl);

		m_timer = new QTimer();
		QObject::connect(m_timer, &QTimer::timeout, [ = ]
		{
			hide();
		});

		hide();
	}

	void ShowWithTimer(int increments)
	{
		m_speedctrl->setValue(increments);

		if (m_parent)
		{
			QRect widgetRect = m_parent->geometry();
			QPoint bl = m_parent->mapToGlobal(widgetRect.bottomLeft());
			move(bl.x(), bl.y());
		}
		show();
		raise();
		m_timer->start(s_timerValue);
	}

private:
	QTimer*           m_timer;
	QPrecisionSlider* m_speedctrl;
	QWidget*          m_parent;

	static const int  s_timerValue;
};

const int CameraSpeedPopup::s_timerValue = 2000;

class CameraSpeedAction : public QWidgetAction
{
public:
	CameraSpeedAction(CRenderViewport* pViewport)
		: QWidgetAction(nullptr)
		, m_widget(nullptr)
		, m_pViewport(pViewport)
	{
	}

protected:
	QWidget* createWidget(QWidget* parent) override
	{
		m_widget = new QWidget(parent);

		m_widget->setObjectName("cameraSpeedViewportMenu");
		m_widget->setAccessibleName("cameraSpeedViewportMenu");

		QBoxLayout* blayout = new QBoxLayout(QBoxLayout::LeftToRight);
		QPrecisionSlider* speedctrl = new QPrecisionSlider(Qt::Horizontal, parent);

		m_widget->setLayout(blayout);

		// we have 9999 steps from 0.01 to 100.0
		// add 0.5 to make sure we include last number (not rounded off due to floating point errors)
		int numsteps = log(9999) / log(1.1) + 0.5;

		speedctrl->setMinimum(0);
		speedctrl->setMaximum(numsteps);
		speedctrl->setTickPosition(QSlider::TicksBelow);
		speedctrl->setTickInterval(numsteps / 20);

		speedctrl->setValue(m_pViewport->GetCameraMoveSpeedIncrements());

		QObject::connect(speedctrl, &QPrecisionSlider::OnSliderChanged, [=](int value)
		{
			m_pViewport->SetCameraMoveSpeedIncrements(value);
		});

		QLabel* speedLabel = new QLabel("Speed");
		speedLabel->setObjectName("cameraSpeedViewportMenuLabel");
		speedLabel->setAccessibleName("cameraSpeedViewportMenuLabel");

		blayout->addWidget(speedLabel);
		blayout->addWidget(speedctrl);

		return m_widget;
	}

	QWidget*         m_widget;
	CRenderViewport* m_pViewport;
};

class QViewportTitleMenu : public QDynamicPopupMenu
{
	QViewportHeader* m_header;
	CLevelEditorViewport*       m_viewport;
public:
	QViewportTitleMenu(QViewportHeader* header, CLevelEditorViewport* viewport) 
		: QDynamicPopupMenu(header)
		, m_viewport(viewport)
		, m_header(header) 
	{
	}

	virtual void OnTrigger(QAction* action) override {}
	virtual void OnPopulateMenu() override
	{
		////////////////////////////////////////////////////////////////////////
		// Process clicks on the view buttons and the menu button
		////////////////////////////////////////////////////////////////////////
		// Only continue when we have a viewport.

		// Create pop up menu.
		CDynamicPopupMenu menu;
		CPopupMenuItem& root = menu.GetRoot();

		if (m_viewport)
		{
			if (m_viewport->IsRenderViewport())
			{
				this->addAction(new CameraSpeedAction(m_viewport));
				this->addSeparator();
			}

			m_viewport->PopulateMenu(root);
		}
		if (!root.Empty())
		{
			root.AddSeparator();
		}

		string curClassName = "";

		CPopupMenuItem& viewsMenu = root.Add("Create Viewport");

		{
			std::vector<IViewPaneClass*> viewPaneClasses;
			int nViews = 0;
			int i;
			std::vector<CViewportClassDesc*> vdesc;
			GetIEditorImpl()->GetViewManager()->GetViewportDescriptions(vdesc);
			for (i = 0; i < vdesc.size(); i++)
			{
				IViewPaneClass* pViewClass = vdesc[i];
				if (!pViewClass || !pViewClass->NeedsMenuItem())
					continue;
				viewPaneClasses.push_back(pViewClass);

				viewsMenu.Add(vdesc[i]->name, functor(*m_header, &QViewportHeader::OnMenuViewSelected), pViewClass) // FIRST_ID_CLASSVIEW+i
				.Check(pViewClass->ClassName() == curClassName);
			}
		}

		menu.PopulateQMenu(this);
	}
};

class QViewportDisplayMenu : public QDynamicPopupMenu
{
private:
	class DisplayOptionsItem : public QWidgetAction
	{
	public:
		DisplayOptionsItem(CLevelEditorViewport* viewport)
			: QWidgetAction(nullptr)
			, m_widget(nullptr)
			, m_pViewport(viewport)
		{
		}

	protected:
		QWidget* createWidget(QWidget* parent)
		{
			CPanelDisplayRender* displayPanel = new CPanelDisplayRender(parent, m_pViewport);
			m_widget = displayPanel;
			return m_widget;
		}

		QWidget*   m_widget;
		CLevelEditorViewport* m_pViewport;
	};

public:
	QViewportDisplayMenu(CLevelEditorViewport* viewport, QWidget* parent = nullptr)
		: QDynamicPopupMenu(parent)
		, m_pViewport(viewport)
	{
	}

	void OnPopulateMenu() override
	{
		this->addAction(new DisplayOptionsItem(m_pViewport));
	}

private:
	CLevelEditorViewport* m_pViewport;
};

QTerrainSnappingMenu::QTerrainSnappingMenu(QToolButton* pToolButton, QViewportHeader* pViewportHeader)
	: QMenu(pToolButton)
	, m_pParentToolButton(pToolButton)
{
	QAction* pTerrainSnap = AddCommandAction("level.toggle_snap_to_terrain", pViewportHeader);
	if (pTerrainSnap)
	{
		pTerrainSnap->setCheckable(true);
		pTerrainSnap->setChecked(GetIEditorImpl()->IsSnapToTerrainEnabled());
		connect(CEditorMainFrame::GetInstance()->m_levelEditor.get(), &CLevelEditor::TerrainSnappingEnabled, this, &QTerrainSnappingMenu::RefreshParentToolButton);
	}
	QAction* pGeometrySnap = AddCommandAction("level.toggle_snap_to_geometry", pViewportHeader);
	if (pGeometrySnap)
	{
		pGeometrySnap->setCheckable(true);
		pGeometrySnap->setChecked(GetIEditorImpl()->IsSnapToGeometryEnabled());
		connect(CEditorMainFrame::GetInstance()->m_levelEditor.get(), &CLevelEditor::GeometrySnappingEnabled, this, &QTerrainSnappingMenu::RefreshParentToolButton);
	}
	addSeparator();
	QAction* pSnapNormal = AddCommandAction("level.toggle_snap_to_surface_normal", pViewportHeader);
	if (pSnapNormal)
	{
		pSnapNormal->setCheckable(true);
		pSnapNormal->setChecked(GetIEditorImpl()->IsSnapToNormalEnabled());
		connect(CEditorMainFrame::GetInstance()->m_levelEditor.get(), &CLevelEditor::SurfaceNormalSnappingEnabled, this, &QTerrainSnappingMenu::RefreshParentToolButton);
	}

	RefreshParentToolButton();
}

QAction* QTerrainSnappingMenu::AddCommandAction(const char* szCommand, QViewportHeader* pViewportHeader)
{
	QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction(szCommand);

	if (!pAction)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, "Command not found");
		return nullptr;
	}

	addAction(pAction);
	connect(pAction, &QAction::triggered, pViewportHeader, &QViewportHeader::SavePersonalization);
	connect(pAction, &QAction::triggered, this, &QTerrainSnappingMenu::RefreshParentToolButton);
	return pAction;
}

void QTerrainSnappingMenu::RefreshParentToolButton()
{
	m_pParentToolButton->setChecked(false);

	if (GetIEditorImpl()->IsSnapToTerrainEnabled())
	{
		m_pParentToolButton->setChecked(true);
		if (GetIEditorImpl()->IsSnapToNormalEnabled())
		{
			m_pParentToolButton->setIcon(CryIcon("icons:common/viewport-snap-terrain-normal.ico"));
			return;
		}
		m_pParentToolButton->setIcon(CryIcon("icons:common/viewport-snap-terrain.ico"));
	}
	else if (GetIEditorImpl()->IsSnapToGeometryEnabled())
	{
		m_pParentToolButton->setChecked(true);
		if (GetIEditorImpl()->IsSnapToNormalEnabled())
		{
			m_pParentToolButton->setIcon(CryIcon("icons:common/viewport-snap-geometry-normal.ico"));
			return;
		}
		m_pParentToolButton->setIcon(CryIcon("icons:common/viewport-snap-geometry.ico"));
	}
	else
	{
		m_pParentToolButton->setIcon(CryIcon("icons:common/viewport-snap-none.ico"));
	}
}

void QTerrainSnappingMenu::mouseReleaseEvent(QMouseEvent* pEvent)
{
	// prevent the menu from closing
	QAction* action = activeAction();
	if (action)
	{
		action->trigger();
	}
}

QViewportHeader::QViewportHeader(CLevelEditorViewport* pViewport)
	: m_searchMode(ESM_BY_NAME)
	, m_searchResultHandling(ESRH_HIDE_OTHERS)
	, m_bOR(false)
	, m_viewport(NULL)
	, m_moduleName("QViewportHeader")
{
	LoadPersonalization();
	m_viewport = pViewport;

	GetIEditorImpl()->RegisterNotifyListener(this);
	CEditorCommandManager* pCommandManager = GetIEditorImpl()->GetCommandManager();

	setContentsMargins(0, 0, 0, 0);
	setFixedHeight(25);

	QBoxLayout* boxlayout = new QBoxLayout(QBoxLayout::LeftToRight);
	boxlayout->setContentsMargins(0, 0, 0, 0);
	boxlayout->setSpacing(0);
	setLayout(boxlayout);

	m_titleBtn = new QToolButton;
	m_titleBtn->setAutoRaise(true);
	m_titleBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
	m_titleBtn->setMenu(new QViewportTitleMenu(this, m_viewport));
	m_titleBtn->setPopupMode(QToolButton::InstantPopup);

	QToolButton* cycleDebugBtn = new QToolButton;
	QAction* cycleDisplayInfo = GetIEditorImpl()->GetICommandManager()->GetAction("general.cycle_displayinfo");
	cycleDebugBtn->setDefaultAction(cycleDisplayInfo);
	ICVar* pCVar = gEnv->pConsole->GetCVar("r_displayInfo");
	m_displayInfoFuncIdx = pCVar->AddOnChangeFunctor(SFunctor([this, cycleDisplayInfo]
	{ this->OnCycleDisplayInfo(cycleDisplayInfo); }));
	OnCycleDisplayInfo(cycleDisplayInfo);
	cycleDebugBtn->setAutoRaise(true);

	m_toggleHelpersBtn = AddToolButtonForCommand("level.toggle_display_helpers", boxlayout);
	m_toggleHelpersBtn->setCheckable(true);
	m_toggleHelpersBtn->setAutoRaise(true);
	m_toggleHelpersBtn->defaultAction()->setChecked(GetIEditorImpl()->IsHelpersDisplayed());

	QToolButton* m_displayOptions = new QToolButton;
	m_displayOptions->setAutoRaise(true);
	m_displayOptions->setMenu(new QViewportDisplayMenu(m_viewport, this));
	m_displayOptions->setPopupMode(QToolButton::InstantPopup);
	m_displayOptions->setText("Display");

	m_pivotSnapping = new QToolButton();
	QAction* pivotSnappingAction = GetIEditorImpl()->GetICommandManager()->GetAction("level.toggle_snap_to_pivot");
	m_pivotSnapping->setDefaultAction(pivotSnappingAction);
	m_pivotSnapping->setToolTip("Pivot Snapping");
	m_pivotSnapping->setIcon(CryIcon("icons:Viewport/viewport-snap-pivot.ico"));
	m_pivotSnapping->setAutoRaise(true);
	m_pivotSnapping->setCheckable(true);

	QToolButton* pTerrainSnapping = new QToolButton();
	pTerrainSnapping->setToolTip("Terrain Snapping");
	pTerrainSnapping->setIcon(CryIcon("icons:common/viewport-snap-terrain.ico"));
	pTerrainSnapping->setCheckable(true);
	pTerrainSnapping->setAutoRaise(true);
	pTerrainSnapping->setMenu(new QTerrainSnappingMenu(pTerrainSnapping, this));
	pTerrainSnapping->setPopupMode(QToolButton::InstantPopup);

	m_currentToolText = new QLabel();
	m_currentToolExit = new QToolButton();
	m_currentToolExit->setToolTip("Exit the current mode");
	m_currentToolExit->setIcon(CryIcon("icons:General/Close.ico"));
	connect(m_currentToolExit, &QToolButton::clicked, []()
	{
		GetIEditorImpl()->SetEditTool(0);
	});
	m_currentToolText->setVisible(false);
	m_currentToolExit->setVisible(false);

	boxlayout->addWidget(m_titleBtn, 1);
	if (pViewport->IsRenderViewport())
	{
		QToolButton* slowbutton = new QToolButton();
		slowbutton->setIcon(CryIcon("icons:Navigation/Camera_Slow.ico"));
		slowbutton->setToolTip("Set Camera speed of the viewport to slow speed");
		connect(slowbutton, &QToolButton::clicked, [=](bool checked)
		{
			int ivalue = log((0.1 - 0.01) * 100) / log(1.1) + 0.5;
			m_viewport->SetCameraMoveSpeedIncrements(ivalue, true);
		});

		QToolButton* mediumbutton = new QToolButton();
		mediumbutton->setIcon(CryIcon("icons:Navigation/Camera_Medium.ico"));
		mediumbutton->setToolTip("Set Camera speed of the viewport to medium speed");
		connect(mediumbutton, &QToolButton::clicked, [=](bool checked)
		{
			int ivalue = log((1.0 - 0.01) * 100) / log(1.1) + 0.5;
			m_viewport->SetCameraMoveSpeedIncrements(ivalue, true);
		});

		QToolButton* fastbutton = new QToolButton();
		fastbutton->setIcon(CryIcon("icons:Navigation/Camera_Fast.ico"));
		fastbutton->setToolTip("Set Camera speed of the viewport to fast speed");
		connect(fastbutton, &QToolButton::clicked, [=](bool checked)
		{
			int ivalue = log((10 - 0.01) * 100) / log(1.1) + 0.5;
			m_viewport->SetCameraMoveSpeedIncrements(ivalue, true);
		});

		boxlayout->addWidget(slowbutton, 1);
		boxlayout->addWidget(mediumbutton, 1);
		boxlayout->addWidget(fastbutton, 1);
	}

	boxlayout->addStretch(1);
	boxlayout->addWidget(m_currentToolText);
	boxlayout->addWidget(m_currentToolExit);
	boxlayout->addStretch(1);
	boxlayout->addWidget(m_displayOptions);
	QToolButton* pSnapToGrid = AddToolButtonForCommand("level.toggle_snap_to_grid", boxlayout);
	pSnapToGrid->setMenu(new QSnapToGridMenu(this));
	pSnapToGrid->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pSnapToGrid, &QToolButton::customContextMenuRequested, pSnapToGrid, &QToolButton::showMenu);
	QToolButton* pSnapToAngle = AddToolButtonForCommand("level.toggle_snap_to_angle", boxlayout);
	pSnapToAngle->setMenu(new QSnapToAngleMenu(this));
	pSnapToAngle->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pSnapToAngle, &QToolButton::customContextMenuRequested, pSnapToAngle, &QToolButton::showMenu);
	QToolButton* pSnapToScale = AddToolButtonForCommand("level.toggle_snap_to_scale", boxlayout);
	pSnapToScale->setMenu(new QSnapToScaleMenu(this));
	pSnapToScale->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pSnapToScale, &QToolButton::customContextMenuRequested, pSnapToScale, &QToolButton::showMenu);
	boxlayout->addWidget(m_pivotSnapping);
	m_vertexSnapping = AddToolButtonForCommand("level.toggle_snap_to_vertex", boxlayout);
	boxlayout->addWidget(pTerrainSnapping);
	boxlayout->addWidget(cycleDebugBtn);
	boxlayout->addWidget(m_toggleHelpersBtn);

	if (pViewport->IsRenderViewport())
	{
		CameraSpeedPopup* cameraSpeedPopup = new CameraSpeedPopup(static_cast<CRenderViewport*>(pViewport), this);
		connect(this, &QViewportHeader::cameraSpeedChanged, [=](float speed, int increments)
		{
			cameraSpeedPopup->ShowWithTimer(increments);
		});
	}

	UpdateCameraName();
}

QViewportHeader::~QViewportHeader()
{
	ICVar* pCVar = gEnv->pConsole->GetCVar("r_displayInfo");
	pCVar->RemoveOnChangeFunctor(m_displayInfoFuncIdx);
	GetIEditorImpl()->UnregisterNotifyListener(this);
	gViewportPreferences.signalSettingsChanged.DisconnectById((uintptr_t)this);
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::SetViewportFOV(float fov)
{
	if (m_viewport && m_viewport->IsRenderViewport())
	{
		m_viewport->SetFOV(DEG2RAD(fov));

		string fovText;
		fovText.Format("FOV:%d", (int)fov);

		// if viewport camera is active, make selected fov new default
		if (GetIEditor()->GetViewManager()->GetCameraObjectId() == CryGUID::Null())
		{
			gViewportPreferences.defaultFOV = DEG2RAD(fov);
			gViewportPreferences.signalSettingsChanged();
		}
	}
}

float QViewportHeader::GetViewportFOV()
{
	if (m_viewport && m_viewport->IsRenderViewport())
	{
		return RAD2DEG(m_viewport->GetFOV());
	}

	return RAD2DEG(gViewportPreferences.defaultFOV);
}

/////////////////////////////////////////////////////////////////////////
void QViewportHeader::AddFOVMenus(CPopupMenuItem& menu, Functor1<float> setFOVFunc, const std::vector<string>& customPresets)
{
	float currentfov = GetViewportFOV();

	static const float fovs[] = {
		55.0f,
		60.0f,
		70.0f,
		80.0f,
		90.0f
	};

	static const size_t fovCount = sizeof(fovs) / sizeof(fovs[0]);

	for (size_t i = 0; i < fovCount; ++i)
	{
		string text;
		text.Format("%.0f", fovs[i]);
		CPopupMenuItem& item = menu.Add(text, setFOVFunc, fovs[i]);

		if (fabs(fovs[i] - currentfov) < 0.1)
		{
			item.Check(true);
		}
	}

	menu.AddSeparator();

	if (!customPresets.empty())
	{
		for (size_t i = 0; i < customPresets.size(); ++i)
		{
			if (customPresets[i].empty())
				break;

			float fov = gViewportPreferences.defaultFOV;
			if (sscanf(customPresets[i], "%f", &fov) == 1)
			{
				fov = std::max(1.0f, fov);
				fov = std::min(120.0f, fov);
				CPopupMenuItem& item = menu.Add(customPresets[i], setFOVFunc, fov);

				if (fabs(fovs[i] - currentfov) < 0.1)
				{
					item.Check(true);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::OnMenuFOVCustom()
{
	QNumericBoxDialog dlg(QObject::tr("Custom FOV"), GetViewportFOV());
	dlg.SetRange(1.0f, 120.0f);
	dlg.RestrictToInt();

	if (dlg.exec() == QDialog::Accepted)
	{
		float fov = dlg.GetValue();

		SetViewportFOV(fov);

		// Update the custom presets.
		string text;
		text.Format("%.0f", fov);
		UpdateCustomPresets(text, m_customFOVPresets);
		SaveCustomPresets("FOVPresets", m_customFOVPresets);
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::AddAspectRatioMenus(CPopupMenuItem& menu, Functor2<unsigned int, unsigned int> setAspectRatioFunc,
                                          const std::vector<string>& customPresets)
{
	static const std::pair<unsigned int, unsigned int> ratios[] = {
		std::pair<unsigned int, unsigned int>(16, 9),
		std::pair<unsigned int, unsigned int>(16, 10),
		std::pair<unsigned int, unsigned int>(4,  3),
		std::pair<unsigned int, unsigned int>(5,  4)
	};

	static const size_t ratioCount = sizeof(ratios) / sizeof(ratios[0]);

	for (size_t i = 0; i < ratioCount; ++i)
	{
		string text;
		text.Format("%d:%d", ratios[i].first, ratios[i].second);
		menu.Add(text, setAspectRatioFunc, ratios[i].first, ratios[i].second);
	}

	menu.AddSeparator();

	if (!customPresets.empty())
	{
		for (size_t i = 0; i < customPresets.size(); ++i)
		{
			if (customPresets[i].empty())
				break;

			unsigned int width;
			unsigned int height;

			if (sscanf(customPresets[i], "%d:%d", &width, &height) == 2)
				menu.Add(customPresets[i], setAspectRatioFunc, width, height);
		}
	}
}

namespace
{
const CRenderViewport::SResolution resolutions[] = {
	CRenderViewport::SResolution(1280, 720),
	CRenderViewport::SResolution(1920, 1080),
	CRenderViewport::SResolution(2560, 1440),
	CRenderViewport::SResolution(2048, 858),
	CRenderViewport::SResolution(1998, 1080),
	CRenderViewport::SResolution(3840, 2160)
};

const size_t resolutionCount = sizeof(resolutions) / sizeof(resolutions[0]);
}

void QViewportHeader::AddResolutionMenus(CPopupMenuItem& menu, Functor2<int, int> resizeFunc, const std::vector<string>& customPresets)
{
	int currentWidth, currentHeight;
	CViewport::EResolutionMode mode = m_viewport->GetResolutionMode();
	m_viewport->GetResolution(currentWidth, currentHeight);

	CPopupMenuItem& item1 = menu.Add("Window Resolution", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::Window);
	item1.Check(mode == CViewport::EResolutionMode::Window);

	for (size_t i = 0; i < resolutionCount; ++i)
	{
		string text;
		int d = gcd(resolutions[i].width, resolutions[i].height);
		int ratio_num = resolutions[i].width / d;
		int ratio_denom = resolutions[i].height / d;

		if (ratio_denom <= 10)
		{
			text.Format("%d x %d (%d:%d)", resolutions[i].width, resolutions[i].height, ratio_num, ratio_denom);
		}
		else
		{
			text.Format("%d x %d (%.2f:1)", resolutions[i].width, resolutions[i].height, ratio_num / (float)ratio_denom);
		}
		CPopupMenuItem& item = menu.Add(text, resizeFunc, resolutions[i].width, resolutions[i].height);

		if (mode != CViewport::EResolutionMode::Window && currentWidth == resolutions[i].width && currentHeight == resolutions[i].height)
		{
			item.Check(true);
		}
	}

	menu.AddSeparator();

	if (!customPresets.empty())
	{
		for (size_t i = 0; i < customPresets.size(); ++i)
		{
			if (customPresets[i].empty())
				break;

			int width, height;

			if (sscanf(customPresets[i], "%d x %d", &width, &height) == 2)
			{
				CPopupMenuItem& item = menu.Add(customPresets[i], resizeFunc, width, height);

				if (currentWidth == width && currentHeight == height)
				{
					item.Check(true);
				}
			}
		}
	}

	menu.AddSeparator();

	CPopupMenuItem& positioningItem = menu.Add("Positioning");

	CPopupMenuItem& itemStretch = positioningItem.Add("Stretch", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::Stretch);
	itemStretch.Check(mode == CViewport::EResolutionMode::Stretch);

	CPopupMenuItem& itemCenter = positioningItem.Add("Center", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::Center);
	itemCenter.Check(mode == CViewport::EResolutionMode::Center);

	CPopupMenuItem& itemTopRight = positioningItem.Add("Top Right", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::TopRight);
	itemTopRight.Check(mode == CViewport::EResolutionMode::TopRight);

	CPopupMenuItem& itemTopLeft = positioningItem.Add("Top Left", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::TopLeft);
	itemTopLeft.Check(mode == CViewport::EResolutionMode::TopLeft);

	CPopupMenuItem& itemBottomRight = positioningItem.Add("Bottom Right", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::BottomRight);
	itemBottomRight.Check(mode == CViewport::EResolutionMode::BottomRight);

	CPopupMenuItem& itemBottomLeft = positioningItem.Add("Bottom Left", functor(*m_viewport, &CViewport::SetResolutionMode), CViewport::EResolutionMode::BottomLeft);
	itemBottomLeft.Check(mode == CViewport::EResolutionMode::BottomLeft);
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::OnMenuResolutionCustom()
{
	int currentWidth, currentHeight;
	m_viewport->GetResolution(currentWidth, currentHeight);

	QCustomResolutionDialog dialog(currentWidth, currentHeight, true);

	if (dialog.exec() == QDialog::Accepted)
	{
		dialog.GetResolution(currentWidth, currentHeight);
		m_viewport->SetResolution(currentWidth, currentHeight);

		for (size_t i = 0; i < resolutionCount; ++i)
		{
			// If resolution already exists in presets, then return
			if (currentWidth == resolutions[i].width && currentHeight == resolutions[i].height)
				return;
		}

		// Update the custom presets.
		string text;
		int d = gcd(currentWidth, currentHeight);
		int ratio_num = currentWidth / d;
		int ratio_denom = currentHeight / d;

		if (ratio_denom <= 10)
		{
			text.Format("%d x %d (%d:%d)", currentWidth, currentHeight, ratio_num, ratio_denom);
		}
		else
		{
			text.Format("%d x %d (%.2f:1)", currentWidth, currentHeight, ratio_num / (float)ratio_denom);
		}
		UpdateCustomPresets(text, m_customResPresets);
		SaveCustomPresets("CustomResPreset", m_customResPresets);
	}
}

void QViewportHeader::OnCycleDisplayInfo(QAction* cycleDisplayInfo)
{
	int currentDisplayInfo = gEnv->pConsole->GetCVar("r_displayInfo")->GetIVal();
	switch (currentDisplayInfo)
	{
	case 0:
		cycleDisplayInfo->setIcon(CryIcon("icons:Viewport/viewport-displayinfo.ico"));
		break;
	case 1:
		cycleDisplayInfo->setIcon(CryIcon("icons:Viewport/viewport-displayinfo_2.ico"));
		break;
	case 2:
		cycleDisplayInfo->setIcon(CryIcon("icons:Viewport/viewport-displayinfo_3.ico"));
		break;
	case 3:
		cycleDisplayInfo->setIcon(CryIcon("icons:Viewport/viewport-displayinfo_1.ico"));
		break;
	default:
		break;
	}
}

void QViewportHeader::OnMenuResolutionCustomClear()
{
	auto ret = CQuestionDialog::SQuestion("Viewport settings", tr("Clear custom resolutions?"));
	if (ret == QDialogButtonBox::Yes)
	{
		m_customResPresets.clear();
		SavePersonalization();
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnDisplayRenderUpdate:
		m_toggleHelpersBtn->setChecked(GetIEditor()->IsHelpersDisplayed());
		break;
	case eNotify_OnEditToolEndChange:
		if (!GetIEditorImpl()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CVertexSnappingModeTool)))
		{
			m_vertexSnapping->setChecked(false);
		}

		if (!GetIEditorImpl()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CPickObjectTool)))
		{
			m_pivotSnapping->setChecked(false);
		}

		// hide the current tool text and button if we're in the root tool mode
		if (!GetIEditorImpl()->GetEditTool() || GetIEditorImpl()->GetEditTool()->IsKindOf(RUNTIME_CLASS(CObjectMode)))
		{
			m_currentToolText->setVisible(false);
			m_currentToolExit->setVisible(false);
		}
		else
		{
			m_currentToolText->setVisible(true);
			m_currentToolExit->setVisible(true);
			m_currentToolText->setText(GetIEditorImpl()->GetEditTool()->GetDisplayName().c_str());
		}

		break;
		
	case eNotify_CameraChanged:
		UpdateCameraName();
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::InputNamesToSearchFromSelection()
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	const CSelectionGroup* pSelected = pObjMgr->GetSelection();
	string names;
	for (int i = 0; i < pSelected->GetCount(); ++i)
	{
		names += pSelected->GetObject(i)->GetName();
		names += " ";
	}
	//m_viewportSearch.SetWindowText(names.c_str());
}

void QViewportHeader::SavePersonalization()
{
	CPersonalizationManager* pPersonalizationManager = GetIEditorImpl()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		CEditorMainFrame* pMainFrame = CEditorMainFrame::GetInstance();
		pPersonalizationManager->SetProperty(m_moduleName, "Grid Snapping", pMainFrame->m_levelEditor->IsGridSnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Angle Snapping", pMainFrame->m_levelEditor->IsAngleSnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Scale Snapping", pMainFrame->m_levelEditor->IsScaleSnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Terrain Snapping", pMainFrame->m_levelEditor->IsTerrainSnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Geometry Snapping", pMainFrame->m_levelEditor->IsGeometrySnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Normal Snapping", pMainFrame->m_levelEditor->IsSurfaceNormalSnappingEnabled());
		pPersonalizationManager->SetProperty(m_moduleName, "Helpers Display", pMainFrame->m_levelEditor->IsHelpersDisplayed());
		SaveCustomPresets("FOVPreset", m_customFOVPresets);
		SaveCustomPresets("AspectRatioPreset", m_customAspectRatioPresets);
		SaveCustomPresets("CustomResPreset", m_customResPresets);
	}
}

void QViewportHeader::LoadPersonalization()
{
	CPersonalizationManager* pPersonalizationManager = GetIEditorImpl()->GetPersonalizationManager();
	if (pPersonalizationManager)
	{
		CEditorMainFrame* pMainFrame = CEditorMainFrame::GetInstance();
		QVariant propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Grid Snapping");
		auto& pLevelEditor = pMainFrame->m_levelEditor;
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableGridSnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Angle Snapping");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableAngleSnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Scale Snapping");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableScaleSnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Terrain Snapping");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableTerrainSnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Geometry Snapping");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableGeometrySnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Normal Snapping");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableSurfaceNormalSnapping(propertyValue.toBool());
		}
		propertyValue = pPersonalizationManager->GetProperty(m_moduleName, "Helpers Display");
		if (propertyValue.isValid())
		{
			pLevelEditor->EnableHelpersDisplay(propertyValue.toBool());
		}
		LoadCustomPresets("FOVPreset", m_customFOVPresets);
		LoadCustomPresets("AspectRatioPreset", m_customAspectRatioPresets);
		LoadCustomPresets("CustomResPreset", m_customResPresets);
	}
}

//////////////////////////////////////////////////////////////////////////
QToolButton* QViewportHeader::AddToolButtonForCommand(const char* szCommand, QLayout* pLayout)
{
	QAction* pAction = GetIEditorImpl()->GetICommandManager()->GetAction(szCommand);

	if (!pAction)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, "Command not found");
		return nullptr;
	}

	QToolButton* pToolButton = new QToolButton();
	connect(pAction, &QAction::triggered, this, &QViewportHeader::SavePersonalization);
	pToolButton->setDefaultAction(pAction);
	pLayout->addWidget(pToolButton);

	return pToolButton;
}

void QViewportHeader::UnhideUnfreezeAll()
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	int objCount = pObjMgr->GetObjectCount();
	CBaseObjectsArray objects;
	pObjMgr->GetObjects(objects);
	for (int i = 0; i < objCount; ++i)
	{
		CBaseObject* pObject = objects[i];
		// Always abide by the layer state before unhiding or unfreezing.
		if (pObject->GetLayer()->IsFrozen() == false)
			pObjMgr->FreezeObject(pObject, false);
		if (pObject->GetLayer()->IsVisible() == true)
			pObjMgr->HideObject(pObject, false);
	}
}

//////////////////////////////////////////////////////////////////////////
static bool DoesTextSatisfyTerms(const string& text, const std::vector<string>& terms, bool bOR)
{
	if (bOR) // OR
	{
		for (size_t i = 0; i < terms.size(); ++i)
		{
			if (text.find(terms[i]) != -1)
				return true;
		}
		return false;
	}
	else // AND
	{
		for (size_t i = 0; i < terms.size(); ++i)
		{
			if (text.find(terms[i]) == -1)
				return false;
		}
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
static void HandleMatched(CBaseObject* pObject)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();

	if (pObject->GetLayer()->IsFrozen() == false)
		pObjMgr->FreezeObject(pObject, false);

	if (pObject->GetLayer()->IsVisible() == true)
		pObjMgr->HideObject(pObject, false);

	if (pObject->GetLayer()->IsFrozen() == false
	    && pObject->GetLayer()->IsVisible() == true)
		pObjMgr->SelectObject(pObject);
}

//////////////////////////////////////////////////////////////////////////
static void HandleFiltered(CBaseObject* pObject, QViewportHeader::ESearchResultHandling searchResultHandling)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	pObjMgr->UnselectObject(pObject);
	if (searchResultHandling == QViewportHeader::ESRH_HIDE_OTHERS)
	{
		if (pObject->GetLayer()->IsFrozen() == false)
			pObjMgr->FreezeObject(pObject, false);
		pObjMgr->HideObject(pObject, true);
	}
	else if (searchResultHandling == QViewportHeader::ESRH_FREEZE_OTHERS)
	{
		if (pObject->GetLayer()->IsVisible() == true)
			pObjMgr->HideObject(pObject, false);
		pObjMgr->FreezeObject(pObject, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::SearchByType(const std::vector<string>& terms)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	int objCount = pObjMgr->GetObjectCount();
	CBaseObjectsArray objects;
	pObjMgr->GetObjects(objects);
	for (int i = 0; i < objCount; ++i)
	{
		CBaseObject* pObject = objects[i];
		string type = pObject->GetTypeDescription();
		type.MakeLower();
		if (DoesTextSatisfyTerms(type, terms, m_bOR))
			HandleMatched(pObject);
		else
			HandleFiltered(pObject, m_searchResultHandling);
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::SearchByName(const std::vector<string>& terms)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	int objCount = pObjMgr->GetObjectCount();
	CBaseObjectsArray objects;
	pObjMgr->GetObjects(objects);
	for (int i = 0; i < objCount; ++i)
	{
		CBaseObject* pObject = objects[i];
		string name = pObject->GetName();
		name.MakeLower();
		if (DoesTextSatisfyTerms(name, terms, m_bOR))
			HandleMatched(pObject);
		else
			HandleFiltered(pObject, m_searchResultHandling);
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::SearchByAsset(const std::vector<string>& terms)
{
	IObjectManager* pObjMgr = GetIEditorImpl()->GetObjectManager();
	int objCount = pObjMgr->GetObjectCount();
	CBaseObjectsArray objects;
	pObjMgr->GetObjects(objects);
	for (int i = 0; i < objCount; ++i)
	{
		CBaseObject* pObject = objects[i];
		CUsedResources usedAssets;
		pObject->GatherUsedResources(usedAssets);
		CUsedResources::TResourceFiles::const_iterator itr = usedAssets.files.begin(),
		                                               end = usedAssets.files.end();
		bool bMatch = false;
		for (; itr != end; ++itr)
		{
			string filename = *itr;
			filename.MakeLower();
			if (DoesTextSatisfyTerms(filename, terms, m_bOR))
			{
				bMatch = true;
				break;
			}
		}
		if (bMatch)
			HandleMatched(pObject);
		else
			HandleFiltered(pObject, m_searchResultHandling);
	}
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::UpdateSearchOptionsText()
{
	/*
	   if(m_searchOptionsText.m_hWnd)
	   {
	   string str = "";

	   if(m_searchMode == ESM_BY_NAME)
	    str += "By Name";
	   else if(m_searchMode == ESM_BY_TYPE)
	    str += "By Type";
	   else //if(m_searchMode == ESM_BY_ASSET)
	    str += "By Asset";

	   if(m_searchResultHandling == ESRH_HIDE_OTHERS)
	    str += ", Hide filtered";
	   else if(m_searchResultHandling == ESRH_FREEZE_OTHERS)
	    str += ", Freeze filtered";
	   else if(m_searchResultHandling == ESRH_JUST_SELECT)
	    str += ", Just Select";

	   if(m_bOR)
	    str += ", OR";
	   else
	    str += ", AND";

	   m_searchOptionsText.SetWindowText(str);
	   }
	 */
}

void QViewportHeader::LoadCustomPresets(const char* keyName, std::vector<string>& outCustompresets)
{
	if (GetIEditorImpl()->GetPersonalizationManager()->HasProperty("QViewportHeader", keyName))
	{
		QVariantList presetsList = GET_PERSONALIZATION_PROPERTY(QViewportHeader, keyName).toList();
		for (const QVariant& preset : presetsList)
			outCustompresets.push_back(preset.toString().toStdString().c_str());
	}
}

void QViewportHeader::SaveCustomPresets(const char* keyName, const std::vector<string>& custompresets)
{
	QVariantList presetsList;
	for (const string& preset : custompresets)
		presetsList.append(preset.c_str());

	SET_PERSONALIZATION_PROPERTY(QViewportHeader, keyName, presetsList);
}

void QViewportHeader::UpdateCustomPresets(const string& text, std::vector<string>& custompresets)
{
	std::vector<string> backup(custompresets);
	custompresets.clear();
	custompresets.push_back(text);
	for (int k = 0, m = 1; k < backup.size() && custompresets.size() <= MAX_NUM_CUSTOM_PRESETS; ++k)
	{
		if (backup[k] != text)
		{
			if (m++ >= custompresets.size())
			{
				custompresets.push_back(backup[k]);
			}
			else
			{
				custompresets[m++] = backup[k];
			}
		}
	}
}

QSize QViewportHeader::sizeHint() const
{
	QFontMetrics fm(font());
	return QSize(14, 14);
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::OnMenuViewSelected(struct IViewPaneClass* viewClass)
{
	string cmd;
	cmd.Format("general.open_pane '%s'", viewClass->ClassName());
	GetIEditorImpl()->ExecuteCommand(cmd.c_str());
}

//////////////////////////////////////////////////////////////////////////
void QViewportHeader::UpdateCameraName()
{
	m_titleBtn->setText(m_viewport->GetCameraMenuName());
}

namespace
{
	void PyToggleHelpers()
	{
		CCryEditApp::GetInstance()->ToggleHelpersDisplay();
	}

	bool PyIsHelpersShown()
	{
		return GetIEditor()->IsHelpersDisplayed();
	}
	void PyCycleDisplayInfo()
	{
		int currentDisplayInfo = gEnv->pConsole->GetCVar("r_displayInfo")->GetIVal();
		switch (currentDisplayInfo)
		{
		case 0:
			gEnv->pConsole->GetCVar("r_displayInfo")->Set(3);
			break;
		case 1:
			gEnv->pConsole->GetCVar("r_displayInfo")->Set(2);
			break;
		case 2:
			gEnv->pConsole->GetCVar("r_displayInfo")->Set(0);
			break;
		case 3:
			gEnv->pConsole->GetCVar("r_displayInfo")->Set(1);
			break;
		default:
			break;
		}
	}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyToggleHelpers, general, toggle_helpers,
	"Toggles the display of helpers.",
	"general.toggle_helpers()");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyIsHelpersShown, general, is_helpers_shown,
	"Gets the display state of helpers.",
	"general.is_helpers_shown()");
REGISTER_PYTHON_COMMAND(PyCycleDisplayInfo, general, cycle_displayinfo, "Cycle display info.");
