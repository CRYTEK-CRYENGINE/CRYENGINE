// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef D3D_RENDER_PIPELINE_H
#define D3D_RENDER_PIPELINE_H

#include "../Common/RenderPipeline.h"

class CD3D9Renderer;
class ICrySizer;

struct SRenderStatePassD3D
{
	uint32                       m_nState;

	CHWShader_D3D::SHWSInstance* m_pShaderVS;
	CHWShader_D3D::SHWSInstance* m_pShaderPS;
};

struct SRenderStateD3D : public SRenderState
{
public:
	TArray<SRenderStatePassD3D> m_Passes;

public:
	void Execute();
};

class CRenderPipelineD3D : public CRenderPipeline
{
public:

public:
	static CRenderPipelineD3D* Create(CD3D9Renderer& renderer)
	{
		return(new CRenderPipelineD3D(renderer));
	}

	int CV_r_renderPipeline;

public:
	virtual void RT_CreateGPUStates(IRenderState* pRootState);
	virtual void RT_ReleaseGPUStates(IRenderState* pRootState);

public:
	~CRenderPipelineD3D();

	void                  FreeMemory();

	int                   GetDeviceDataSize();
	void                  ReleaseDeviceObjects();
	HRESULT               RestoreDeviceObjects();
	void                  GetMemoryUsage(ICrySizer* pSizer) const;

	virtual SRenderState* CreateRenderState();

	void*                 operator new(size_t s)
	{
		uint8* p = (uint8*) malloc(s + 16 + 8);
		memset(p, 0, s + 16 + 8);
		uint8* pRet = (uint8*) ((size_t) (p + 16 + 8) & ~0xF);
		((uint8**) pRet)[-1] = p;
		return pRet;
	}

	void operator delete(void* p)
	{
		free(((uint8**)p)[-1]);
	}

private:
	CRenderPipelineD3D(CD3D9Renderer& renderer);

private:
	CD3D9Renderer& m_renderer;

};

#endif // D3D_RENDER_PIPELINE_H
