// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QWidget>

class SANDBOX_API QPreviewWidget : public QWidget, public IEditorNotifyListener
{
public:
	QPreviewWidget(const QString& modelFile = QString(), QWidget* pParent = nullptr);
	virtual ~QPreviewWidget();

	void            LoadFile(const QString& modelFile, const bool changeCamera = true);
	void            LoadParticleEffect(const QString& filePath);
	void            LoadParticleEffect(IParticleEffect* pEffect);
	void            LoadMaterial(const QString& materialFile);
	const Vec3&     GetSize() const;
	const QString&  GetLoadedFile() const;

	void            SetEntity(IRenderNode* pEntity);
	void            SetObject(IStatObj* pObject);
	IStatObj*       GetObject() const;
	void            SetCameraLookAt(const float radiusScale, const Vec3& dir = Vec3(0, 1, 0));
	void            SetCameraRadius(const float radius);
	const CCamera&  GetCamera() const;
	void            SetGrid(const bool bEnable);
	void            SetAxis(const bool bEnable);
	void            SetRotation(const bool bEnable);
	void            SetClearColor(const QColor& color);

	//TODO: transparency and background texture don't work as they used to in the MFC version of the preview widget
	void            SetBackgroundTexture(const QString& textureFilename);
	void            UseBackLight(const bool bEnable);
	bool            UseBackLight() const;
	void            SetShowNormals(const bool bShow);
	void            SetShowPhysics(const bool bShow);
	void            SetShowRenderInfo(const bool bShow);

	void            EnableUpdate(const bool bEnable);
	bool            IsUpdateEnabled() const;
	void            IdleUpdate(const bool bForceUpdate = false);
	void            ProcessKeys();

	void            SetMaterial(CMaterial* pMaterial);
	CMaterial*      GetMaterial() const;

	//! Renders to an image
	QImage          GetImage();
	void			SavePreview(const char* outFile);
	const Matrix34& GetCameraTM() const;
	void            SetCameraTM(const Matrix34& cameraTM);

	// Place camera so that whole object fits on screen.
	void FitToScreen();

	// Get information about the preview model.
	std::size_t GetFaceCount() const;
	std::size_t GetVertexCount() const;
	std::size_t GetMaxLod() const;
	std::size_t GetMaterialCount() const;

	void        UpdateAnimation();

	void        SetShowObject(const bool bShowObject);
	bool        GetShowObject() const;

	void        SetAmbient(const QColor& ambientColor);
	void        SetAmbientMultiplier(const float multiplier);

	// Get the character instance.
	ICharacterInstance* GetCharacter() const;

	void EnableMaterialPrecaching(const bool bPrecacheMaterial);
	void EnableWireframeRendering(const bool bDrawWireframe);

	bool CreateContext();
	void ReleaseObject();
	void DeleteRenderContex();

	// QWidget interface
public:
	virtual QPaintEngine* paintEngine() const override;
	virtual QSize         sizeHint() const override;

protected:
	IRenderer* GetRenderer() const { return m_pRenderer; }
	CCamera&   GetCamera()         { return m_camera; }

	// QWidget interface
protected:
	virtual void paintEvent(QPaintEvent* pEvent) override;
	virtual void mousePressEvent(QMouseEvent* pEvent) override;
	virtual void mouseReleaseEvent(QMouseEvent* pEvent) override;
	virtual void mouseMoveEvent(QMouseEvent* pEvent) override;
	virtual void wheelEvent(QWheelEvent* pEvent) override;
	virtual void contextMenuEvent(QContextMenuEvent* event) override;
	virtual void resizeEvent(QResizeEvent *) override;

	// IEditorNotifyListener interface
protected:
	virtual void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

protected:
	virtual void RenderObject(IMaterial* pMaterial, const SRenderingPassInfo& passInfo);

private:
	void OnLButtonDown(const QPoint& point);
	void OnLButtonUp(const QPoint& point);
	void OnMButtonDown(const QPoint& point);
	void OnMButtonUp(const QPoint& point);
	void OnRButtonUp(const QPoint& point);
	void OnRButtonDown(const QPoint& point);

	bool Render();
	void SetCamera(const CCamera& camera);

	void DrawGrid();
	void DrawBackground();

private:
	enum class Mode
	{
		None,
		Rotate,
		Pan,
		Zoom
	};

	SDisplayContextKey m_displayContextKey;

protected:
	CCamera                  m_camera;
	float                    m_fov;
	_smart_ptr<IStatObj>     m_pObject;
	ICharacterInstance*      m_pCharacter;
	IRenderer*               m_pRenderer;
	ICharacterManager*       m_pAnimationSystem;
	bool                     m_bContextCreated;
	Vec3                     m_size;
	QString                  m_loadedFile;
	std::vector<SRenderLight>     m_lights;
	AABB                     m_aabb;
	Vec3                     m_cameraTarget;
	float                    m_cameraRadius;
	Vec3                     m_cameraAngles;
	Mode                     m_mode;
	QPoint                   m_mousePosition;
	IRenderNode*             m_pEntity;
	struct IParticleEmitter* m_pEmitter;
	bool                     m_bHaveAnythingToRender;
	bool                     m_bGrid;
	bool                     m_bAxis;
	bool                     m_bUpdate;
	bool                     m_bRotate;
	float                    m_rotateAngle;
	QColor                   m_clearColor;
	QColor                   m_ambientColor;
	float                    m_ambientMultiplier;
	bool                     m_bUseBacklight;
	bool                     m_bShowObject;
	bool                     m_bPrecacheMaterial;
	bool                     m_bDrawWireFrame;
	bool                     m_bShowNormals;
	bool                     m_bShowPhysics;
	bool                     m_bShowRenderInfo;
	int                      m_backgroundTextureId;
	_smart_ptr<CMaterial>    m_pCurrentMaterial;
	bool					 m_bDoNotShowContextMenu;
	int                      m_width = 0;
	int                      m_height = 0;
};

