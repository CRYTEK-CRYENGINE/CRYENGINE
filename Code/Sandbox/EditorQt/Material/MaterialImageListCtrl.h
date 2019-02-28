// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __MaterialImageListCtrl_h__
#define __MaterialImageListCtrl_h__
#pragma once

#include "Controls\ImageListCtrl.h"
#include "Controls\PreviewModelCtrl.h"
#include "Material.h"
#include <CryRenderer/IRenderer.h> // IAsyncTextureCompileListener

enum EMILC_ModelType
{
	EMMT_Default = 0,
	EMMT_Box,
	EMMT_Sphere,
	EMMT_Teapot,
	EMMT_Plane
};

//////////////////////////////////////////////////////////////////////////
class CMaterialImageListCtrl : public CImageListCtrl, private IAsyncTextureCompileListener, public ITextureStreamListener
{
public:
	typedef Functor1<CImageListCtrlItem*> SelectCallback;

	struct CMtlItem : public CImageListCtrlItem
	{
		_smart_ptr<CMaterial> pMaterial;
		std::vector<string>  vVisibleTextures;
	};

	CMaterialImageListCtrl();
	~CMaterialImageListCtrl();

	CImageListCtrlItem* AddMaterial(CMaterial* pMaterial, void* pUserData = NULL);
	void                SetMaterial(int nItemIndex, CMaterial* pMaterial, void* pUserData = NULL);
	void                SelectMaterial(CMaterial* pMaterial);
	CMtlItem*           FindMaterialItem(CMaterial* pMaterial);
	void                InvalidateMaterial(CMaterial* pMaterial);

	void                SetSelectMaterialCallback(SelectCallback func) { m_selectMtlFunc = func; };

	void                DeleteAllItems() override;

protected:
	afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

	void         LoadModel();
	virtual void OnSelectItem(CImageListCtrlItem* pItem, bool bSelected);
	virtual void OnUpdateItem(CImageListCtrlItem* pItem);
	virtual void CalcLayout(bool bUpdateScrollBar = true);

private:
	// Compile listener
	virtual void OnCompilationStarted(const char* source, const char* target, int nPending);
	virtual void OnCompilationFinished(const char* source, const char* target, ERcExitCode eReturnCode);

	virtual void OnCompilationQueueTriggered(int nPending) {}
	virtual void OnCompilationQueueDepleted() {}

	// Stream listener
	virtual void OnCreatedStreamedTexture(void* pHandle, const char* name, int nMips, int nMinMipAvailable) {}
	virtual void OnUploadedStreamedTexture(void* pHandle);
	virtual void OnDestroyedStreamedTexture(void* pHandle) {}

	virtual void OnTextureWantsMip(void* pHandle, int nMinMip) {}
	virtual void OnTextureHasMip(void* pHandle, int nMinMip) {}

	virtual void OnBegunUsingTextures(void** pHandles, size_t numHandles) {}
	virtual void OnEndedUsingTextures(void** pHandle, size_t numHandles) {}
private:
	_smart_ptr<CMaterial> m_pMatPreview;

	CPreviewModelCtrl     m_renderCtrl;
	IStatObj*             m_pStatObj;
	SelectCallback        m_selectMtlFunc;
	EMILC_ModelType       m_nModel;
	int                   m_nColor;
};

#endif //__MaterialImageListCtrl_h__

