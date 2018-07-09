// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>
#include "QPreviewWidget.h"

#include "Material/Material.h"
#include "Material/MaterialManager.h"
#include "IIconManager.h"
#include "ViewportInteraction.h"
#include <Preferences/ViewportPreferences.h>
#include "Objects/ParticleEffectObject.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryParticleSystem/IParticles.h>
#include <QtUtil.h>
#include <QDir.h>
#include <QFileInfo>
#include <QImage>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QCursor>

#include "RenderLock.h"
#include "FilePathUtil.h"

namespace Private_PreviewWidget
{
struct MaterialId
{
	MaterialId(const void* pPtr, int id)
		: pPtr(pPtr)
		, id(id)
	{}

	bool operator<(const MaterialId& otherId) const
	{
		return pPtr < otherId.pPtr || id < otherId.id;
	}

	const void* pPtr;
	int         id;
};

template<typename T>
QColor operator*(T weight, const QColor& color)
{
	QColor result;
	result.setRedF(weight * color.redF());
	result.setGreenF(weight * color.greenF());
	result.setBlueF(weight * color.blueF());
	result.setAlphaF(weight * color.alphaF());
	return result;
}

template<typename T>
QColor operator*(const QColor& color, T weight)
{
	return operator*(weight, color);
}

template<typename ResultT, typename CountCurrentObjectT, typename AccumulateSubObjectT>
ResultT GetCountRecursively(IStatObj* pObject, CountCurrentObjectT countCurrentObject, AccumulateSubObjectT accumulateSubObject)
{
	if (!pObject)
	{
		return 0;
	}
	ResultT result = countCurrentObject(pObject);
	for (int i = 0; i < pObject->GetSubObjectCount(); ++i)
	{
		auto pSubObject = pObject->GetSubObject(i);
		if (pSubObject)
		{
			result = accumulateSubObject(result, pSubObject->pStatObj);
		}
	}
	return result;
}

std::size_t GetFaceCountRecursively(IStatObj* pObject)
{
	const auto countCurrent = [](IStatObj* pCurrentObject)
	{
		if (pCurrentObject->GetRenderMesh())
		{
			return pCurrentObject->GetRenderMesh()->GetIndicesCount() / 3;
		}
		return 0;
	};
	const auto accumulateSubObject = [](std::size_t previousResult, IStatObj* pNextObject) { return previousResult + GetFaceCountRecursively(pNextObject); };
	return GetCountRecursively<std::size_t>(pObject, countCurrent, accumulateSubObject);
}

std::size_t GetVertexCountRecursively(IStatObj* pObject)
{
	const auto countCurrent = [](IStatObj* pCurrentObject)
	{
		if (pCurrentObject->GetRenderMesh())
		{
			return pCurrentObject->GetRenderMesh()->GetVerticesCount();
		}
		return 0;
	};
	const auto accumulateSubObject = [](std::size_t previousResult, IStatObj* pNextObject) { return previousResult + GetVertexCountRecursively(pNextObject); };
	return GetCountRecursively<std::size_t>(pObject, countCurrent, accumulateSubObject);
}

std::size_t GetMaxLodRecursively(IStatObj* pObject)
{
	const auto countCurrent = [](IStatObj* pCurrentObject)
	{
		std::size_t lod = 0;
		for (std::size_t i = 1; i < MAX_STATOBJ_LODS_NUM; ++i)
		{
			if (pCurrentObject->GetLodObject(i))
			{
				lod = i;
			}
		}
		return lod;
	};
	const auto accumulateSubObject = [](std::size_t previousResult, IStatObj* pNextObject)
	{
		auto subLod = GetMaxLodRecursively(pNextObject);
		previousResult = (previousResult < subLod) ? subLod : previousResult;
		return previousResult;
	};
	return GetCountRecursively<std::size_t>(pObject, countCurrent, accumulateSubObject);
}

std::size_t GetMaterialCountRecursively(IStatObj* pObject)
{
	const auto countCurrent = [](IStatObj* pCurrentObject)
	{
		if (pCurrentObject->GetRenderMesh())
		{
			const auto& chunks = pCurrentObject->GetRenderMesh()->GetChunks();
			return chunks.size();
		}
		return 0;
	};
	const auto accumulateSubObject = [](std::size_t previousResult, IStatObj* pNextObject) { return previousResult + GetMaterialCountRecursively(pNextObject); };
	return GetCountRecursively<std::size_t>(pObject, countCurrent, accumulateSubObject);
}

// from CPanelPreview
IParticleEffect* FindUpperValidParticleEffect(const QString& effectName)
{
	if (effectName.isEmpty())
	{
		return nullptr;
	}

	auto pEffect = gEnv->pParticleManager->FindEffect(effectName.toStdString().c_str());
	if (pEffect)
	{
		return pEffect;
	}

	QString upperEffectName(effectName);
	const auto dotPos = upperEffectName.lastIndexOf('.');
	if (dotPos == -1)
	{
		return nullptr;
	}
	upperEffectName = upperEffectName.remove(dotPos, upperEffectName.size() - dotPos);

	return FindUpperValidParticleEffect(upperEffectName);
}

IParticleEffect* FindValidParticleEffect(IParticleEffect* pParentEffect, const QString& effectName)
{
	if (!pParentEffect)
	{
		return nullptr;
	}

	for (int i = 0; i < pParentEffect->GetChildCount(); ++i)
	{
		auto pItem = pParentEffect->GetChild(i);
		if (effectName == pItem->GetName())
		{
			return pItem;
		}
		pItem = FindValidParticleEffect(pItem, effectName);
		if (pItem)
		{
			return pItem;
		}
	}

	return nullptr;
}
}

QPreviewWidget::QPreviewWidget(const QString& modelFile, QWidget* pParent)
	: QWidget(pParent)
	, m_fov(60)
	, m_pCharacter(nullptr)
	, m_pRenderer(GetIEditorImpl()->GetRenderer())
	, m_pAnimationSystem(GetIEditorImpl()->GetSystem()->GetIAnimationSystem())
	, m_bContextCreated(false)
	, m_size(0, 0, 0)
	, m_aabb(2)
	, m_cameraTarget(0, 0, 0)
	, m_cameraRadius(0)
	, m_cameraAngles(0, 0, 0)
	, m_mode(Mode::None)
	, m_pEntity(nullptr)
	, m_pEmitter(nullptr)
	, m_bHaveAnythingToRender(false)
	, m_bGrid(true)
	, m_bAxis(true)
	, m_bUpdate(false)
	, m_bRotate(false)
	, m_rotateAngle(0)
	, m_clearColor(QColor::fromRgbF(0.5f, 0.5f, 0.5f, 1.0f))
	, m_ambientColor(QColor::fromRgbF(1.0f, 1.0f, 1.0f, 1.0f))
	, m_ambientMultiplier(0.5f)
	, m_bUseBacklight(false)
	, m_bShowObject(true)
	, m_bPrecacheMaterial(true)
	, m_bDrawWireFrame(false)
	, m_bShowNormals(false)
	, m_bShowPhysics(false)
	, m_bShowRenderInfo(false)
	, m_backgroundTextureId(0)
	, m_bDoNotShowContextMenu(true)
	, m_width(256)
	, m_height(256)
{
	m_camera.SetFrustum(800, 600, DEG2RAD(m_fov), 0.02f, 10000.0f);

	SRenderLight light;
	const float lightColor = 1.0f;
	light.m_Flags |= DLF_SUN | DLF_DIRECTIONAL;
	light.SetRadius(10000);
	light.SetLightColor(ColorF(lightColor, lightColor, lightColor, lightColor));
	light.SetPosition(Vec3(100, 100, 100));
	m_lights.push_back(light);

	FitToScreen();

	GetIEditorImpl()->RegisterNotifyListener(this);

	setFocusPolicy(Qt::ClickFocus);
	setWindowFlags(Qt::MSWindowsOwnDC);
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NativeWindow);

	if (!modelFile.isEmpty())
	{
		LoadFile(modelFile);
	}
}

QPreviewWidget::~QPreviewWidget()
{
	DeleteRenderContex();
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

bool QPreviewWidget::CreateContext()
{
	// Create context.
	if (m_pRenderer && !m_bContextCreated)
	{
		IRenderer::SDisplayContextDescription desc;

		desc.handle = reinterpret_cast<HWND>(winId());
		desc.type = IRenderer::eViewportType_Secondary;
		desc.clearColor = ColorF(m_clearColor.redF(), m_clearColor.greenF(), m_clearColor.blueF());
		desc.renderFlags = FRT_CLEAR_COLOR | FRT_CLEAR_DEPTH | FRT_TEMPORARY_DEPTH;
		desc.screenResolution.x = m_width;
		desc.screenResolution.y = m_height;

		m_displayContextKey = m_pRenderer->CreateSwapChainBackedContext(desc);
		m_bContextCreated = true;

		return true;
	}

	return false;
}

void QPreviewWidget::ReleaseObject()
{
	m_pObject = nullptr;
	SAFE_RELEASE(m_pCharacter);
	if (m_pEmitter)
	{
		m_pEmitter->Activate(false);
		m_pEmitter->Release();
		m_pEmitter = nullptr;
	}
	m_pEntity = nullptr;
	m_bHaveAnythingToRender = false;
}

void QPreviewWidget::LoadFile(const QString& modelFile, const bool bChangeCamera)
{
	m_bHaveAnythingToRender = false;
	if (!m_pRenderer)
	{
		return;
	}

	ReleaseObject();

	if (modelFile.isEmpty())
	{
		update();
		return;
	}

	m_loadedFile = modelFile;

	const auto strFileExt = QFileInfo(modelFile).suffix();
	const auto isSKEL = strFileExt.compare(QLatin1String(CRY_SKEL_FILE_EXT), Qt::CaseInsensitive) == 0;
	const auto isSKIN = strFileExt.compare(QLatin1String(CRY_SKIN_FILE_EXT), Qt::CaseInsensitive) == 0;
	const auto isCDF = strFileExt.compare(QLatin1String(CRY_CHARACTER_DEFINITION_FILE_EXT), Qt::CaseInsensitive) == 0;
	const auto isCGA = strFileExt.compare(QLatin1String(CRY_ANIM_GEOMETRY_FILE_EXT), Qt::CaseInsensitive) == 0;
	const auto isCGF = strFileExt.compare(QLatin1String(CRY_GEOMETRY_FILE_EXT), Qt::CaseInsensitive) == 0;

	if (isCGA)
	{
		// Load CGA animated object.
		m_pCharacter = m_pAnimationSystem->CreateInstance(modelFile.toStdString().c_str());
		if (!m_pCharacter)
		{
			Warning("Loading of geometry object %s failed.", modelFile.toStdString().c_str());
			update();
			return;
		}
		m_pCharacter->AddRef();
		m_aabb = m_pCharacter->GetAABB();
	}

	if (isSKEL || isSKIN || isCDF)
	{
		// Load character.
		m_pCharacter = m_pAnimationSystem->CreateInstance(modelFile.toStdString().c_str(), CA_PreviewMode | CA_CharEditModel);
		if (!m_pCharacter)
		{
			Warning("Loading of character %s failed.", modelFile.toStdString().c_str());
			update();
			return;
		}
		m_pCharacter->AddRef();
		m_aabb = m_pCharacter->GetAABB();
	}

	if (isCGF)
	{
		// Load object.
		m_pObject = GetIEditorImpl()->Get3DEngine()->LoadStatObj(modelFile.toStdString().c_str(), nullptr, nullptr, false);
		if (!m_pObject)
		{
			Warning("Loading of geometry object %s failed.", modelFile.toStdString().c_str());
			update();
			return;
		}
		m_aabb.min = m_pObject->GetBoxMin();
		m_aabb.max = m_pObject->GetBoxMax();
	}
	else
	{
		update();
		return;
	}

	m_bHaveAnythingToRender = true;

	if (bChangeCamera)
	{
		FitToScreen();
	}

	update();
}

void QPreviewWidget::LoadParticleEffect(const QString& effectName)
{
	using namespace Private_PreviewWidget;

	if (effectName.isEmpty())
	{
		return;
	}

	if (CParticleEffectObject::IsGroup(effectName.toStdString().c_str()))
	{
		return;
	}

	_smart_ptr<IParticleEffect> pEffect = gEnv->pParticleManager->FindEffect(effectName.toStdString().c_str());
	if (!pEffect)
	{
		auto pParentEffect = FindUpperValidParticleEffect(effectName);
		if (!pParentEffect)
		{
			return;
		}
		pEffect = FindValidParticleEffect(pParentEffect, effectName);
		if (!pEffect)
		{
			return;
		}
	}
	LoadParticleEffect(pEffect);
}

void QPreviewWidget::LoadParticleEffect(IParticleEffect* pEffect)
{
	m_bHaveAnythingToRender = false;
	if (!m_pRenderer)
	{
		return;
	}
	ReleaseObject();

	if (!pEffect)
	{
		return;
	}

	const auto clientWidth = width();
	const auto clientHeight = height();
	if (clientHeight < 2 || clientWidth < 2)
	{
		return;
	}

	Matrix34 matrix;
	matrix.SetIdentity();
	matrix.SetRotationXYZ(Ang3(DEG2RAD(90.0f), 0, 0));

	m_bHaveAnythingToRender = true;
	SpawnParams spawnParams;
	spawnParams.bNowhere = true;
	m_pEmitter = pEffect->Spawn(matrix, spawnParams);
	if (m_pEmitter)
	{
		m_pEmitter->AddRef();
		m_pEmitter->Update();
		m_aabb = m_pEmitter->GetBBox();
		if (m_aabb.IsReset())
		{
			m_aabb = AABB(1);
		}
		FitToScreen();
	}

	update();
}

void QPreviewWidget::LoadMaterial(const QString& materialFile)
{
	auto pMaterial = GetIEditorImpl()->GetMaterialManager()->LoadMaterial(materialFile.toStdString().c_str());
	SetMaterial(pMaterial);
}

const Vec3& QPreviewWidget::GetSize() const
{
	return m_size;
}

const QString& QPreviewWidget::GetLoadedFile() const
{
	return m_loadedFile;
}

void QPreviewWidget::SetEntity(IRenderNode* pEntity)
{
	m_bHaveAnythingToRender = false;
	if (m_pEntity != pEntity)
	{
		m_pEntity = pEntity;
		if (m_pEntity)
		{
			m_bHaveAnythingToRender = true;
			m_aabb = m_pEntity->GetBBox();
		}
		update();
	}
}

void QPreviewWidget::SetObject(IStatObj* pObject)
{
	if (m_pObject != pObject)
	{
		m_bHaveAnythingToRender = false;
		m_pObject = pObject;
		if (m_pObject)
		{
			m_bHaveAnythingToRender = true;
			m_aabb = m_pObject->GetAABB();
		}
		update();
	}
}

IStatObj* QPreviewWidget::GetObject() const
{
	return m_pObject;
}

void QPreviewWidget::SetCameraRadius(const float radius)
{
	m_cameraRadius = radius;

	const Matrix34& cameraMatrix = m_camera.GetMatrix();
	const Vec3 direction = cameraMatrix.TransformVector(Vec3(0, 1, 0));
	Matrix34 transformationMatrix = Matrix33::CreateRotationVDir(direction, 0);
	transformationMatrix.SetTranslation(m_cameraTarget - direction * m_cameraRadius);
	m_camera.SetMatrix(transformationMatrix);
}

void QPreviewWidget::SetCameraLookAt(const float radiusScale, const Vec3& fromDir)
{
	m_cameraTarget = m_aabb.GetCenter();
	m_cameraRadius = m_aabb.GetRadius() * radiusScale;

	const Vec3 direction = fromDir.GetNormalized();
	Matrix34 matrix = Matrix33::CreateRotationVDir(direction, 0);
	matrix.SetTranslation(m_cameraTarget - direction * m_cameraRadius);
	m_camera.SetMatrix(matrix);
}

const CCamera& QPreviewWidget::GetCamera() const
{
	return m_camera;
}

void QPreviewWidget::SetGrid(const bool bEnable)
{
	m_bGrid = bEnable;
}

void QPreviewWidget::SetAxis(const bool bEnable)
{
	m_bAxis = bEnable;
}

void QPreviewWidget::UseBackLight(const bool bEnable)
{
	if (bEnable)
	{
		m_lights.resize(1);
		SRenderLight light;
		light.SetPosition(Vec3(-100, 100, -100));
		float lightColor = 0.5f;
		light.m_Flags |= DLF_POINT;
		light.SetLightColor(ColorF(lightColor, lightColor, lightColor, lightColor));
		light.SetRadius(1000);
		m_lights.push_back(light);
	}
	else
	{
		m_lights.resize(1);
	}
	m_bUseBacklight = bEnable;
}

bool QPreviewWidget::UseBackLight() const
{
	return m_bUseBacklight;
}

void QPreviewWidget::SetShowNormals(const bool bShow)
{
	m_bShowNormals = bShow;
}

void QPreviewWidget::SetShowPhysics(const bool bShow)
{
	m_bShowPhysics = bShow;
}

void QPreviewWidget::SetShowRenderInfo(const bool bShow)
{
	m_bShowRenderInfo = bShow;
}

void QPreviewWidget::paintEvent(QPaintEvent* pEvent)
{
	bool res = Render();
	if (!res)
	{
		QPainter painter(this);
		painter.fillRect(rect(), QColor(128, 128, 128));
	}
	pEvent->accept();
}

void QPreviewWidget::mousePressEvent(QMouseEvent* pEvent)
{
	auto localMousePos = pEvent->pos();
	switch (pEvent->button())
	{
	case Qt::LeftButton:
		OnLButtonDown(localMousePos);
		break;

	case Qt::RightButton:
		OnRButtonDown(localMousePos);
		break;

	case Qt::MiddleButton:
		OnMButtonDown(localMousePos);
		break;

	default:
		break;
	}

	setCursor(Qt::BlankCursor);
	pEvent->accept();
}

void QPreviewWidget::mouseReleaseEvent(QMouseEvent* pEvent)
{
	auto localMousePos = pEvent->pos();
	switch (pEvent->button())
	{
	case Qt::LeftButton:
		OnLButtonUp(localMousePos);
		break;

	case Qt::RightButton:
		OnRButtonUp(localMousePos);
		break;

	case Qt::MiddleButton:
		OnMButtonUp(localMousePos);
		break;

	default:
		break;
	}

	unsetCursor();
	pEvent->accept();
}

void QPreviewWidget::mouseMoveEvent(QMouseEvent* pEvent)
{
	auto point = pEvent->pos();
	if (point == m_mousePosition || m_mode == Mode::None)
	{
		return;
	}

	switch (m_mode)
	{
	case Mode::Pan:
		{
			// Slide.
			const float speedScale = 0.5f;
			Matrix34 cameraMatrix = m_camera.GetMatrix();
			const Vec3 xDirection = cameraMatrix.GetColumn0().GetNormalized();
			const Vec3 zDirection = cameraMatrix.GetColumn2().GetNormalized();
			Vec3 position = m_cameraTarget;

			position += 0.1f * xDirection * (point.x() - m_mousePosition.x()) * speedScale + 0.1f * zDirection * (m_mousePosition.y() - point.y()) * speedScale;
			m_cameraTarget = position;

			Vec3 direction = cameraMatrix.TransformVector(Vec3(0, 1, 0));
			cameraMatrix.SetTranslation(m_cameraTarget - direction * m_cameraRadius);
			m_camera.SetMatrix(cameraMatrix);

			break;
		}

	case Mode::Zoom:
		{
			// Zoom.
			const Matrix34& cameraMatrix = m_camera.GetMatrix();
			const Vec3 xDirection(0, 0, 0);
			const Vec3 yDirection = cameraMatrix.GetColumn1().GetNormalized();

			const float step = 0.5f;
			const float xDifference = (point.x() - m_mousePosition.x());
			const float yDifference = (point.y() - m_mousePosition.y());

			m_camera.SetPosition(m_camera.GetPosition() + step * xDirection * xDifference + step * yDirection * yDifference);
			SetCamera(m_camera);

			m_bDoNotShowContextMenu = true;

			break;
		}

	case Mode::Rotate:
		{
			Vec3 position = m_camera.GetMatrix().GetTranslation();
			m_cameraRadius = Vec3(m_camera.GetMatrix().GetTranslation() - m_cameraTarget).GetLength();
			// Look
			Ang3 angles(-point.y() + m_mousePosition.y(), 0, -point.x() + m_mousePosition.x());
			angles *= 0.002f;

			Matrix34 cameraMatrix = m_camera.GetMatrix();
			const Matrix33 zRotMatrix = Matrix33::CreateRotationXYZ(Ang3(0, 0, angles.z));               // Rotate around vertical axis.
			const Matrix33 xRotMatrix = Matrix33::CreateRotationAA(angles.x, cameraMatrix.GetColumn0()); // Rotate with angle around x axis in camera space.
			const Vec3 direction = cameraMatrix.TransformVector(Vec3(0, 1, 0));
			const Vec3 newDirection = (xRotMatrix * zRotMatrix).TransformVector(direction).GetNormalized();

			cameraMatrix = Matrix34(Matrix33::CreateRotationVDir(newDirection, 0), m_cameraTarget - newDirection * m_cameraRadius);
			m_camera.SetMatrix(cameraMatrix);

			break;
		}

	default:
		break;
	}

	// capture the mouse to stay centered. Use "m_mousePosition = point;" to disable
	m_mousePosition = QPoint(width() / 2, height() / 2);
	const auto cursorPos = mapToGlobal(m_mousePosition);
	QCursor::setPos(cursorPos);
	update();

	pEvent->accept();
}

void QPreviewWidget::wheelEvent(QWheelEvent* pEvent)
{
	// TODO: Add your message handler code here and/or call default
	const Matrix34& cameraMatrix = m_camera.GetMatrix();
	const Vec3 zDirection = cameraMatrix.GetColumn1().GetNormalized();

	//m_camera.SetPosition( m_camera.GetPos() + ydir*(m_mousePos.y-point.y),xdir*(m_mousePos.x-point.x) );
	const auto delta = pEvent->angleDelta().y();
	m_camera.SetPosition(m_camera.GetPosition() + 0.02f * zDirection * delta);
	SetCamera(m_camera);
	update();

	pEvent->accept();
}

void QPreviewWidget::contextMenuEvent(QContextMenuEvent* event)
{
	if (m_bDoNotShowContextMenu)
	{
		event->accept(); //accepting the event will prevent it from being passed on to parent widgets
		m_bDoNotShowContextMenu = false;
	}
	else
	{
		//This will ignore the event
		QWidget::contextMenuEvent(event);
	}
}

void QPreviewWidget::resizeEvent(QResizeEvent *ev)
{
	QWidget::resizeEvent(ev);

	int cx = ev->size().width() * devicePixelRatioF();
	int cy = ev->size().height() * devicePixelRatioF();
	if (cx == 0 || cy == 0)
		return;

	//ignore sizes when widget is invisible, just used to render pixmaps
	if (isVisible()) 
	{
		m_width = cx;
		m_height = cy;
	}

	gEnv->pRenderer->ResizeContext(m_displayContextKey, m_width, m_height);
}

void QPreviewWidget::SetCamera(const CCamera& camera)
{
	m_camera.SetPosition(camera.GetPosition());

	const auto frustumWidth = width();
	const auto frustumHeight = height();

	m_camera.SetFrustum(frustumWidth, frustumHeight, DEG2RAD(m_fov), m_camera.GetNearPlane(), m_camera.GetFarPlane());
}

bool QPreviewWidget::Render()
{
	const auto clientWidth = width() * devicePixelRatioF();
	const auto clientHeight = height() * devicePixelRatioF();
	if (clientHeight < 2 || clientWidth < 2)
	{
		return false;
	}

	if (CScopedRenderLock lock = CScopedRenderLock())
	{
		if (!m_bContextCreated)
		{
			if (!CreateContext())
			{
				return false;
			}
		}

		float tod = GetIEditorImpl()->GetCurrentMissionTime();
		GetIEditorImpl()->SetCurrentMissionTime(12.0f);

		SetCamera(m_camera);

		m_pRenderer->BeginFrame(m_displayContextKey);

		DrawBackground();
		if (m_bGrid || m_bAxis)
		{
			DrawGrid();
		}

		// save some cvars
		int showNormals = gEnv->pConsole->GetCVar("r_ShowNormals")->GetIVal();
		int showPhysics = gEnv->pConsole->GetCVar("p_draw_helpers")->GetIVal();
		int showInfo = gEnv->pConsole->GetCVar("r_displayInfo")->GetIVal();

		gEnv->pConsole->GetCVar("r_ShowNormals")->Set((int)m_bShowNormals);
		gEnv->pConsole->GetCVar("p_draw_helpers")->Set((int)m_bShowPhysics);
		gEnv->pConsole->GetCVar("r_displayInfo")->Set((int)m_bShowRenderInfo);

		// Render object.
		SRenderingPassInfo passInfo = SRenderingPassInfo::CreateGeneralPassRenderingInfo(m_camera, SRenderingPassInfo::DEFAULT_FLAGS, true, m_displayContextKey);
		m_pRenderer->EF_StartEf(passInfo);

		{
			CScopedWireFrameMode scopedWireFrame(m_pRenderer, m_bDrawWireFrame ? R_WIREFRAME_MODE : R_SOLID_MODE);

			// Add lights.
			for (std::size_t i = 0; i < m_lights.size(); ++i)
			{
				m_pRenderer->EF_ADDDlight(&m_lights[i], passInfo);
			}

			if (m_pCurrentMaterial)
			{
				m_pCurrentMaterial->DisableHighlight();
			}

			_smart_ptr<IMaterial> pMaterial;
			if (m_pCurrentMaterial)
			{
				pMaterial = m_pCurrentMaterial->GetMatInfo();
			}

			if (m_bPrecacheMaterial)
			{
				auto pCurrentMaterial = pMaterial;
				if (!pCurrentMaterial)
				{
					if (m_pObject)
					{
						pCurrentMaterial = m_pObject->GetMaterial();
					}
					else if (m_pEntity)
					{
						pCurrentMaterial = m_pEntity->GetMaterial();
					}
					else if (m_pCharacter)
					{
						pCurrentMaterial = m_pCharacter->GetIMaterial();
					}
					else if (m_pEmitter)
					{
						pCurrentMaterial = m_pEmitter->GetMaterial();
					}
				}

				if (pCurrentMaterial)
				{
					pCurrentMaterial->PrecacheMaterial(0.0f, nullptr, true, true);
				}
			}

			bool bNoPreview = false;
			{
				// activate shader item
				auto pCurrentMaterial = pMaterial;
				if (!pCurrentMaterial)
				{
					if (m_pObject)
					{
						pCurrentMaterial = m_pObject->GetMaterial();
					}
					else if (m_pEntity)
					{
						pCurrentMaterial = m_pEntity->GetMaterial();
					}
					else if (m_pCharacter)
					{
						pCurrentMaterial = m_pCharacter->GetIMaterial();
					}
					else if (m_pEmitter)
					{
						pCurrentMaterial = m_pEmitter->GetMaterial();
					}
				}
				// ActivateAllShaderItem();

				if (pCurrentMaterial)
				{
					if ((pCurrentMaterial->GetFlags() & MTL_FLAG_NOPREVIEW))
					{
						bNoPreview = true;
					}
				}
			}

			if (m_bShowObject && !bNoPreview)
			{
				RenderObject(pMaterial, passInfo);
			}

			m_pRenderer->EF_EndEf3D(SHDF_NOASYNC | SHDF_ALLOWHDR | SHDF_SECONDARY_VIEWPORT, -1, -1, passInfo);
		}

		m_pRenderer->RenderDebug(false);
		m_pRenderer->EndFrame();

		gEnv->pConsole->GetCVar("r_ShowNormals")->Set(showNormals);
		gEnv->pConsole->GetCVar("p_draw_helpers")->Set(showPhysics);
		gEnv->pConsole->GetCVar("r_displayInfo")->Set(showInfo);
		GetIEditorImpl()->SetCurrentMissionTime(tod);
	}

	return true;
}

void QPreviewWidget::RenderObject(IMaterial* pMaterial, const SRenderingPassInfo& passInfo)
{
	using namespace Private_PreviewWidget;

	SRendParams renderParams;
	renderParams.dwFObjFlags = 0;

	auto scaledColor = m_ambientColor * m_ambientMultiplier;
	renderParams.AmbientColor = ColorF(scaledColor.redF(), scaledColor.greenF(), scaledColor.blueF(), scaledColor.alphaF());
	renderParams.dwFObjFlags |= FOB_TRANS_MASK /*| FOB_GLOBAL_ILLUMINATION*/ | FOB_NO_FOG /*| FOB_ZPREPASS*/;
	renderParams.pMaterial = pMaterial;

	Matrix34 matrix;
	matrix.SetIdentity();
	renderParams.pMatrix = &matrix;

	if (m_bRotate)
	{
		matrix.SetRotationXYZ(Ang3(0, 0, m_rotateAngle));
		m_rotateAngle += 0.1f;
	}

	if (m_pObject)
	{
		m_pObject->Render(renderParams, passInfo);
	}

	if (m_pEntity)
	{
		m_pEntity->Render(renderParams, passInfo);
	}

	if (m_pCharacter)
	{
		m_pCharacter->Render(renderParams, passInfo);
	}

	if (m_pEmitter)
	{
		m_pEmitter->Update();
		m_pEmitter->Render(renderParams, passInfo);
	}
}

void QPreviewWidget::DrawGrid()
{
	IRenderAuxGeom* pRag = m_pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags rendFlags = pRag->GetRenderFlags();

	pRag->SetRenderFlags(e_Def3DPublicRenderflags);
	SAuxGeomRenderFlags newFlags = pRag->GetRenderFlags();
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	pRag->SetRenderFlags(newFlags);

	int gridAlpha = 40;
	const float xLimit = 5;
	const float yLimit = 5;
	if (m_bGrid)
	{
		const float step = 0.1f;
		// Draw grid.
		for (float x = -xLimit; x < xLimit; x += step)
		{
			if (fabs(x) > 0.01)
			{
				pRag->DrawLine(Vec3(x, -yLimit, 0), ColorB(150, 150, 150, gridAlpha), Vec3(x, yLimit, 0), ColorB(150, 150, 150, gridAlpha));
			}
		}

		for (float y = -yLimit; y < yLimit; y += step)
		{
			if (fabs(y) > 0.01)
			{
				pRag->DrawLine(Vec3(-xLimit, y, 0), ColorB(150, 150, 150, gridAlpha), Vec3(xLimit, y, 0), ColorB(150, 150, 150, gridAlpha));
			}
		}
	}

	gridAlpha = 60;
	if (m_bAxis)
	{
		// Draw axis.
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(255, 0, 0, gridAlpha), Vec3(xLimit, 0, 0), ColorB(255, 0, 0, gridAlpha));
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(0, 255, 0, gridAlpha), Vec3(0, yLimit, 0), ColorB(0, 255, 0, gridAlpha));
		pRag->DrawLine(Vec3(0, 0, 0), ColorB(0, 0, 255, gridAlpha), Vec3(0, 0, yLimit), ColorB(0, 0, 255, gridAlpha));
	}
	pRag->SetRenderFlags(rendFlags);
}

void QPreviewWidget::UpdateAnimation()
{
	if (!m_pCharacter)
	{
		return;
	}

	GetISystem()->GetIAnimationSystem()->Update(false);
	m_pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(0);

	const float distance = m_camera.GetPosition().GetLength();
	const float zoomFactor = 0.001f + 0.999f * (RAD2DEG(m_camera.GetFov()) / 60.f);

	SAnimationProcessParams animationParams;
	animationParams.locationAnimation = QuatTS(IDENTITY);
	animationParams.bOnRender = 0;
	animationParams.zoomAdjustedDistanceFromCamera = distance * zoomFactor;
	m_pCharacter->StartAnimationProcessing(animationParams);
	m_pCharacter->FinishAnimationComputations();

	m_aabb = m_pCharacter->GetAABB();
}

void QPreviewWidget::SetShowObject(const bool bShowObject)
{
	m_bShowObject = bShowObject;
}

bool QPreviewWidget::GetShowObject() const
{
	return m_bShowObject;
}

void QPreviewWidget::SetAmbient(const QColor& ambientColor)
{
	m_ambientColor = ambientColor;
}

void QPreviewWidget::SetAmbientMultiplier(const float multiplier)
{
	m_ambientMultiplier = multiplier;
}

ICharacterInstance* QPreviewWidget::GetCharacter() const
{
	return m_pCharacter;
}

void QPreviewWidget::EnableMaterialPrecaching(const bool bPrecacheMaterial)
{
	m_bPrecacheMaterial = bPrecacheMaterial;
}

void QPreviewWidget::EnableWireframeRendering(const bool bDrawWireframe)
{
	m_bDrawWireFrame = bDrawWireframe;
}

void QPreviewWidget::SetCameraTM(const Matrix34& cameraTM)
{
	m_camera.SetMatrix(cameraTM);
}

const Matrix34& QPreviewWidget::GetCameraTM() const
{
	return m_camera.GetMatrix();
}

void QPreviewWidget::DeleteRenderContex()
{
	ReleaseObject();

	// Destroy render context.
	if (m_pRenderer && m_bContextCreated)
	{
		// Do not delete primary context.
		if (m_displayContextKey != reinterpret_cast<HWND>(m_pRenderer->GetHWND()))
			m_pRenderer->DeleteContext(m_displayContextKey);
		m_bContextCreated = false;
	}
}

QPaintEngine* QPreviewWidget::paintEngine() const
{
	return nullptr;
}

QSize QPreviewWidget::sizeHint() const
{
	return QSize(320, 240);
}

void QPreviewWidget::OnLButtonDown(const QPoint& point)
{
	m_mousePosition = point;
	m_mode = Mode::Rotate;
	grabMouse();
	update();
}

void QPreviewWidget::OnLButtonUp(const QPoint& point)
{
	m_mode = Mode::None;
	releaseMouse();
	update();
}

void QPreviewWidget::OnMButtonDown(const QPoint& point)
{
	m_mousePosition = point;
	m_mode = Mode::Pan;
	grabMouse();
	update();
}

void QPreviewWidget::OnMButtonUp(const QPoint& point)
{
	m_mode = Mode::None;
	releaseMouse();
	update();
}

void QPreviewWidget::OnRButtonDown(const QPoint& point)
{
	m_mousePosition = point;
	m_mode = Mode::Zoom;
	grabMouse();
	update();
}

void QPreviewWidget::OnRButtonUp(const QPoint& point)
{
	m_mode = Mode::None;
	releaseMouse();
	update();
}

void QPreviewWidget::EnableUpdate(const bool bUpdate)
{
	m_bUpdate = bUpdate;
}

bool QPreviewWidget::IsUpdateEnabled() const
{
	return m_bUpdate;
}

void QPreviewWidget::IdleUpdate(const bool bForceUpdate)
{
	if (!isVisible())
		return;

	if (hasFocus())
	{
		ProcessKeys();
	}

	if (m_bUpdate && m_bHaveAnythingToRender || bForceUpdate)
	{
		update();
	}
}

void QPreviewWidget::SetRotation(const bool bEnable)
{
	m_bRotate = bEnable;
}

void QPreviewWidget::SetMaterial(CMaterial* pMaterial)
{
	if (pMaterial)
	{
		if ((pMaterial->GetFlags() & MTL_FLAG_NOPREVIEW))
		{
			m_pCurrentMaterial = nullptr;
			update();
			return;
		}
	}
	m_pCurrentMaterial = pMaterial;
	update();
}

CMaterial* QPreviewWidget::GetMaterial() const
{
	return m_pCurrentMaterial;
}

void QPreviewWidget::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnIdleUpdate:
		IdleUpdate();
		break;
	}
}

void QPreviewWidget::SavePreview(const char* outFile)
{
	if (m_pCurrentMaterial)
	{
		m_pCurrentMaterial->LoadShader();
		if (m_pCurrentMaterial->GetMatInfo())
		{
			m_pCurrentMaterial->GetMatInfo()->ForceTexturesLoading(max(width(), height()));
		}
	}

	if (m_pObject && m_pObject->GetMaterial())
	{
		m_pObject->GetMaterial()->ForceTexturesLoading(max(width(), height()));
	}

	// Flush pending commands to make sure SShaderItem::m_nPreprocessFlags is updated
	m_pRenderer->FlushRTCommands(true, true, true);

	Render();

	QDir().mkpath(QtUtil::ToQString(PathUtil::GetPathWithoutFilename(outFile)));
	m_pRenderer->ScreenShot(outFile, m_displayContextKey);
}

QImage QPreviewWidget::GetImage()
{
	const auto clientWidth = width();
	const auto clientHeight = height();
	QImage img(clientWidth, clientHeight, QImage::Format_RGB888);

	if (m_pObject && m_pObject->GetMaterial())
	{
		m_pObject->GetMaterial()->ForceTexturesLoading(max(width(), height()));
	}

	m_pRenderer->EnableSwapBuffers(false);
	Render();
	m_pRenderer->EnableSwapBuffers(true);

	m_pRenderer->ReadFrameBuffer((uint32*)img.bits(), clientWidth, clientHeight);

	return img;

	//This approach doesn't work, presumably because this widget is not painted in a regular fashion
	/*
	QPixmap pixmap(size());
	render(&pixmap);
	return pixmap;
	*/
}

void QPreviewWidget::SetClearColor(const QColor& color)
{
	m_clearColor = color;
}

std::size_t QPreviewWidget::GetFaceCount() const
{
	using namespace Private_PreviewWidget;

	if (m_pObject)
	{
		return GetFaceCountRecursively(m_pObject);
	}
	// empty m_pCharacter branch
	return 0;
}

std::size_t QPreviewWidget::GetVertexCount() const
{
	using namespace Private_PreviewWidget;

	if (m_pObject)
	{
		return GetVertexCountRecursively(m_pObject);
	}
	return 0;
}

std::size_t QPreviewWidget::GetMaxLod() const
{
	using namespace Private_PreviewWidget;

	if (m_pObject)
	{
		return GetMaxLodRecursively(m_pObject);
	}
	else if (m_pCharacter)
	{
		return 1; //BaseModels have only 1 LOD
	}
	return 0;
}

std::size_t QPreviewWidget::GetMaterialCount() const
{
	using namespace Private_PreviewWidget;

	if (m_pObject)
	{
		return GetMaterialCountRecursively(m_pObject);
	}
	return 0;
}

void QPreviewWidget::FitToScreen()
{
	SetCameraLookAt(2.0f, Vec3(1, 1, -0.5));
}

void QPreviewWidget::ProcessKeys()
{
	float speedScale = 60.0f * GetIEditorImpl()->GetSystem()->GetITimer()->GetFrameTime();
	if (speedScale > 20.0f)
	{
		speedScale = 20.0f;
	}
	speedScale *= 0.04f;

	if (QtUtil::IsModifierKeyDown(Qt::ShiftModifier))
	{
		speedScale *= gViewportMovementPreferences.camFastMoveSpeed;
	}

	const Matrix34& cameraMatrix = m_camera.GetMatrix();
	const Vec3 yDirection = cameraMatrix.GetColumn2().GetNormalized();
	const Vec3 xDirection = cameraMatrix.GetColumn0().GetNormalized();
	const Vec3 position = cameraMatrix.GetTranslation();
	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Forward))
	{
		// move forward
		m_camera.SetPosition(position + speedScale * yDirection);
		SetCamera(m_camera);
	}

	if (ViewportInteraction::CheckPolledKey(ViewportInteraction::eKey_Backward))
	{
		// move backward
		m_camera.SetPosition(position - speedScale * yDirection);
		SetCamera(m_camera);
	}
}

void QPreviewWidget::SetBackgroundTexture(const QString& textureFilename)
{
	m_backgroundTextureId = GetIEditorImpl()->GetIconManager()->GetIconTexture(textureFilename.toStdString().c_str());
}

void QPreviewWidget::DrawBackground()
{
	if (!m_backgroundTextureId)
	{
		return;
	}

	const int clientWidth = width();
	const int clientHeight = height();

	IRenderAuxGeom::GetAux()->SetOrthographicProjection(true, 0.0f, clientWidth, clientHeight, 0.0f, 0.0f, 1.0f);

	auto flags = IRenderAuxGeom::GetAux()->GetRenderFlags();
	auto oldFlags = flags;
	flags.SetDepthTestFlag(e_DepthTestOff);
	IRenderAuxGeom::GetAux()->SetRenderFlags(flags);
	IRenderAuxImage::Draw2dImage(0,0,clientWidth, clientHeight, m_backgroundTextureId, 0.f,0.f,1.f,1.f,0.f,ColorF(1.f,1.f,1.f,1.f));
	IRenderAuxGeom::GetAux()->SetRenderFlags(oldFlags);

	//m_pRenderer->DrawImageWithUV(0, 0, 0.5f, clientWidth, clientHeight, m_backgroundTextureId, uvs, uvt, color[0], color[1], color[2], color[3]);
	IRenderAuxGeom::GetAux()->SetOrthographicProjection(false);
}

