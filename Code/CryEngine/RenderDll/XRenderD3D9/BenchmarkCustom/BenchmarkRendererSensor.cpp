// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef ENABLE_BENCHMARK_SENSOR
	#include "BenchmarkRendererSensor.h"
	#include <D3DPostProcess.h>
	#include <IHardwareInfoProvider.h>
	#include <DriverD3D.h>
	#include <IBenchmarkRendererSensorManager.h>
	#include <IBenchmarkFramework.h>
	#include <HighPrecisionTimer.h>
	#include <IRenderResultsObserver.h>

	#include <CryCore/Platform/CryLibrary.h>

static BenchmarkFramework::IBenchmarkFramework* s_instance = 0;

static BenchmarkFramework::IBenchmarkFramework* GetFrameworkInstance()
{
	if (!s_instance)
	{
		HMODULE hlib = CryLoadLibrary("BenchmarkFramework.dll");
		if (hlib)
		{
			FUNCTIONPOINTER_GetGlobalBenchmarkFrameworkInstance pf = (FUNCTIONPOINTER_GetGlobalBenchmarkFrameworkInstance)CryGetProcAddress(hlib, "GetGlobalBenchmarkFrameworkInstance");
			if (pf)
			{
				s_instance = pf();
			}
		}
	}
	return s_instance;
}

BenchmarkRendererSensor::BenchmarkRendererSensor(CD3D9Renderer* renderer)
	: m_samples(0, 200),
	m_timer(new BenchmarkFramework::HighPrecisionTimer),
	m_renderer(renderer),
	m_resultObserver(0),
	m_currentState(TestInactive),
	m_shaderUniformName("SourceSize"),
	m_flickerFunc(0),
	/*m_matrixTexHandle(0),
	   m_matrixTexWidth(200),
	   m_matrixTexHeight(200),
	   m_rows(4),
	   m_columns(4),
	   m_currentIndex(0),*/
	m_currentFrameCount(0),
	m_frameFlickerID(0),
	m_frameSampleFlags(0),
	m_frameSampleInterval(0)
	//m_matrixTexNeedsRefresh(true),
	//m_matrixTexNeedsClear(false),

{
	if (GetFrameworkInstance())
		GetFrameworkInstance()->getRendererSensorManager()->setBenchmarkRendererSensor(this);
}

BenchmarkRendererSensor::~BenchmarkRendererSensor()
{
	/*if(m_matrixTexHandle)
	   {
	   m_renderer->DestroyRenderTarget(m_matrixTexHandle);
	   m_matrixTexHandle = 0;
	   }*/
	if (GetFrameworkInstance())
	{
		GetFrameworkInstance()->getRendererSensorManager()->setBenchmarkRendererSensor(0);
	}
	SAFE_DELETE(m_timer);
	s_instance = 0;
}

void BenchmarkRendererSensor::startTest()
{
	if (gRenDev->m_pRT->IsRenderThread())
	{
		if (m_currentState != TestInactive)
		{
			return;
		}

		m_currentFrameCount = 0;
		initializeTest();
		m_currentState = TestRunning;

	}
	else
	{
		CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
		byte* ptr = queue.pushCommand(StartTest, 0);
	}

}

void BenchmarkRendererSensor::endTest()
{
	if (gRenDev->m_pRT->IsRenderThread())
	{
		if (m_currentState == TestInactive)
		{
			return;
		}

		m_renderer->EnableSwapBuffers(true);
		m_currentState = TestInactive;
		submitResults();
		cleanupAfterTestRun();

	}
	else
	{
		CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
		byte* ptr = queue.pushCommand(EndTest, 0);
	}
}

/*void BenchmarkRendererSensor::setMatrixTextureResolution(int w, int h)
   {
   if(gRenDev->m_pRT->IsRenderThread())
   {
    m_matrixTexWidth = w;
    m_matrixTexHeight = h;
    m_matrixTexNeedsRefresh = true;
   } else {
    CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
    byte* ptr = queue.pushCommand(SetMatrixTextureResolution, sizeof(int) * 2);
    queue.pushData(ptr, w);
    queue.pushData(ptr, h);
   }

   }*/
void BenchmarkRendererSensor::setFrameSampling(int interval, int flags)
{
	if (gRenDev->m_pRT->IsRenderThread())
	{
		m_frameSampleInterval = interval;
		m_frameSampleFlags = flags;
	}
	else
	{
		CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
		byte* ptr = queue.pushCommand(SetFrameSampling, sizeof(int) * 2);
		queue.pushData(ptr, interval);
		queue.pushData(ptr, flags);
	}

}

void BenchmarkRendererSensor::submitResults()
{
	if (m_resultObserver)
	{
		CRY_ASSERT(!m_samples.empty());
		BenchmarkFramework::TimeStamp currentTime = m_timer->getTimestamp();
		BenchmarkFramework::RenderResultsStruct results;
		results.framesCount = m_currentFrameCount;
		results.startTime = m_startedTime;
		results.endTime = m_samples[m_samples.Num() - 1].afterFrameSubmitTimestamp;
		results.sampleCount = m_samples.Num();
		results.samples = m_samples.Data();
		m_resultObserver->submitResults(results, gRenDev->m_pRT->m_nCurThreadProcess);
		m_samples.Free();
	}
}

void BenchmarkRendererSensor::cleanupAfterTestRun()
{

}

void BenchmarkRendererSensor::setRenderResultsObserver(BenchmarkFramework::IRenderResultsObserver* observer)
{
	m_resultObserver = observer;
}
/*void BenchmarkRendererSensor::setMatrixDimensions(int rows, int columns)
   {

   if(gRenDev->m_pRT->IsRenderThread())
   {
    m_rows = rows <= 0 ? 1: rows;
    m_columns = columns <= 0 ? 1: columns;
    m_currentIndex = 0;
    m_matrixTexNeedsClear = true;
   } else {
    CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
    byte* ptr = queue.pushCommand(SetMatrixDimensions, sizeof(int) * 2);
    queue.pushData(ptr, rows);
    queue.pushData(ptr, columns);
   }

   }



   void BenchmarkRendererSensor::recreateMatrixTexture()
   {

   if(m_matrixTexHandle)
   {
    m_renderer->DestroyRenderTarget(m_matrixTexHandle);
   }
   m_matrixTexHandle = m_renderer->CreateRenderTarget(m_matrixTexWidth, m_matrixTexHeight); //CTexture::CreateRenderTarget("$FrameDebugMatrix",m_matrixTexWidth, m_matrixTexHeight, eTT_2D, FT_DONT_STREAM | FT_USAGE_RENDERTARGET | FT_DONT_RESIZE | FT_NOMIPS, eTF_R8G8B8A8);

   m_matrixTexNeedsRefresh = false;
   m_matrixTexNeedsClear = true;
   }

   void BenchmarkRendererSensor::clearMatrixTexture()
   {

   m_matrixTexNeedsClear = false;
   }*/

void BenchmarkRendererSensor::update()
{
	processCommands();

}

void BenchmarkRendererSensor::overrideMonitorWindowSize(bool override, int width, int height, bool fullscreen)
{
	if (gRenDev->m_pRT->IsRenderThread())
	{
		m_renderer->OverrideWindowParameters(override, width, height, fullscreen);
	}
	else
	{
		CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
		byte* ptr = queue.pushCommand(ForceWindowSize, sizeof(int) * 4);
		queue.pushData(ptr, override ? 1 : 0);
		queue.pushData(ptr, width);
		queue.pushData(ptr, height);
		queue.pushData(ptr, fullscreen ? 1 : 0);

	}

}

void BenchmarkRendererSensor::enableFlicker(bool enable, BenchmarkFramework::FlickerFunction func)
{

	if (gRenDev->m_pRT->IsRenderThread())
	{

		m_flickerFunc = enable ? func : nullptr;
		m_frameFlickerID = 0;

	}
	else
	{
		CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadFill];
		byte* ptr = queue.pushCommand(SetupFlicker, sizeof(int) + sizeof(BenchmarkFramework::FlickerFunction));
		queue.pushData(ptr, enable ? 1 : 0);
		queue.pushData(ptr, func);

	}
}

void BenchmarkRendererSensor::processCommands()
{
	CommandQueue& queue = m_commandQueue[gRenDev->m_pRT->m_nCurThreadProcess];
	while (!queue.isQueueEmpty())
	{
		FrameMatrixCommand command = queue.popCommand();
		switch (command)
		{
		case StartTest:
			{
				startTest();
			}
			break;
		case EndTest:
			{
				endTest();
			}
			break;
		case ForceWindowSize:
			{
				bool enable = queue.popData<int>() > 0;
				int width = queue.popData<int>();
				int height = queue.popData<int>();
				bool enableFullscreen = queue.popData<int>() != 0;
				overrideMonitorWindowSize(enable, width, height, enableFullscreen);
			}
			break;
		case SetupFlicker:
			{
				int enable = queue.popData<int>();
				BenchmarkFramework::FlickerFunction func = queue.popData<BenchmarkFramework::FlickerFunction>();
				enableFlicker(enable != 0, func);
			}
			break;

		/*case SetMatrixTextureResolution:
		   {
		   int width = queue.popData<int>();
		   int height = queue.popData<int>();
		   setMatrixTextureResolution(width, height);
		   }
		   break;
		   case SetMatrixDimensions:
		   {
		   int rows = queue.popData<int>();
		   int columns = queue.popData<int>();
		   setMatrixDimensions(rows, columns);
		   }
		   break;*/
		case SetFrameSampling:
			{
				int interval = queue.popData<int>();
				int flags = queue.popData<int>();
				setFrameSampling(interval, flags);
			}
			break;
		}
		;
	}
	queue.reset();
}

bool BenchmarkRendererSensor::shouldPresentFrame()
{
	return true;//m_currentIndex == 0u;
}

void BenchmarkRendererSensor::initializeTest()
{

	/*if(m_matrixTexNeedsRefresh)
	   {
	   recreateMatrixTexture();
	   }
	   if(m_matrixTexNeedsClear)
	   {
	   clearMatrixTexture();
	   }*/

	m_startedTime = m_timer->getTimestamp();
}

void BenchmarkRendererSensor::forceFlush(CCryDeviceWrapper& device, CCryDeviceContextWrapper& context)
{
	context.Flush();

	ID3D11Query* query;
	BOOL data;

	D3D11_QUERY_DESC desc;
	desc.Query = D3D11_QUERY_EVENT;
	desc.MiscFlags = 0;

	device.CreateQuery(&desc, &query);
	context.End(query);

	HRESULT hr;
	do
	{
		hr = context.GetData(query, (void*)&data, sizeof(BOOL), 0);
	}
	while (hr == S_FALSE);

}

/*void BenchmarkRendererSensor::copyFrameToMatrix(CTexture* texture)
   {
   int ox, oy, ow, oh;
   m_renderer->GetViewport(&ox, &oy, &ow, &oh);

   //render to matrixTex
   m_renderer->SetRenderTarget(m_matrixTexHandle);

   int vpW = m_matrixTexWidth / m_columns;
   int vpH = m_matrixTexHeight / m_rows;

   int indX = m_currentIndex  % m_columns;
   int indY = m_currentIndex / m_columns;

   m_renderer->SetViewport(vpW * indX, vpH * indY, vpW, vpH);

   PostProcessUtils().CopyTextureToScreen(texture);

   m_renderer->SetViewport(ox,oy,ow,oh);
   m_renderer->SetRenderTarget(0);
   }
   void BenchmarkRendererSensor::copyStereoFrameToMatrix(CTexture* left, CTexture* right)
   {

   //render the frames to matrix
   int ox, oy, ow, oh;
   m_renderer->GetViewport(&ox, &oy, &ow, &oh);

   m_renderer->SetRenderTarget(m_matrixTexHandle);

   int vpW = m_matrixTexWidth / m_columns;
   int vpH = m_matrixTexHeight / m_rows;

   int indX = m_currentIndex  % m_columns;
   int indY = m_currentIndex / m_columns;

   m_renderer->RT_SetViewport(vpW * indX, vpH * indY, vpW, vpH);

   CShader *shader = m_renderer->m_cEF.s_ShaderStereo;

   shader->FXSetTechnique("SideBySide");
   uint32 nPasses = 0;
   shader->FXBegin(&nPasses, FEF_DONTSETSTATES);
   shader->FXBeginPass(0);

   m_renderer->FX_SetState(GS_NODEPTHTEST);

   int width = m_renderer->GetWidth();
   int height = m_renderer->GetHeight();


   Vec4 pParams= Vec4((float) width, (float) height, 0, 0);
   CShaderMan::s_shPostEffects->FXSetPSFloat(m_shaderUniformName, &pParams, 1);

   GetUtils().SetTexture(left, 0, FILTER_LINEAR);
   GetUtils().SetTexture(right, 1, FILTER_LINEAR);

   GetUtils().DrawFullScreenTri(width, height);



   shader->FXEndPass();
   shader->FXEnd();

   m_renderer->SetViewport(ox,oy,ow,oh);
   m_renderer->SetRenderTarget(0);
   }
 */
void BenchmarkRendererSensor::copyFrameToScreen(CTexture* texture)
{
	// TODO: Check if this still works with the new pipeline
	m_strechRectPass.Execute(texture, m_renderer->GetActiveColorOutput());
}

void BenchmarkRendererSensor::copyStereoFrameToScreen(CTexture* left, CTexture* right)
{
	// TODO: Check if this still works with the new pipeline
	m_renderer->m_cEF.mfRefreshSystemShader("Stereo", CShaderMan::s_ShaderStereo);
	CShader* pShader = m_renderer->m_cEF.s_ShaderStereo;

	int width = m_renderer->GetWidth();
	int height = m_renderer->GetHeight();
	
	Vec4 pParams = Vec4((float)width, (float)height, 0, 0);

	m_copyPass.SetTechnique(pShader, "SideBySide", 0);
	m_copyPass.SetRenderTarget(0, m_renderer->GetActiveColorOutput());
	m_copyPass.SetState(GS_NODEPTHTEST);
	m_copyPass.SetTextureSamplerPair(0, left, EDefaultSamplerStates::LinearClamp);
	m_copyPass.SetTextureSamplerPair(1, right, EDefaultSamplerStates::LinearClamp);
	m_copyPass.SetPrimitiveType(CRenderPrimitive::ePrim_ProceduralTriangle);
	
	m_copyPass.BeginConstantUpdate();
	m_copyPass.SetConstant(m_shaderUniformName, pParams);
	m_copyPass.Execute();
}

bool BenchmarkRendererSensor::isActive()
{
	return m_currentState != TestInactive;
}

void BenchmarkRendererSensor::applyFlicker(CTexture* left, CTexture* right, bool leftWhite, bool rightWhite)
{
	float lum = leftWhite ? 1.f : 0.f;
	blendTextureWithColor(left, lum, lum, lum, 0.95f);
	lum = rightWhite ? 1.f : 0.f;
	blendTextureWithColor(right, lum, lum, lum, 0.95f);
}

void BenchmarkRendererSensor::blendTextureWithColor(CTexture* tex, float r, float g, float b, float a)
{
	ASSERT_LEGACY_PIPELINE
		/*
	m_renderer->FX_PushRenderTarget(0, tex, 0, 0);
	m_renderer->FX_SetActiveRenderTargets();
	//SStateBlend blendState;

	//apply blending to see a little bit of the original rendertarget data
	//m_renderer->FX_SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
	GetUtils().ClearScreen(r, g, b, a);

	m_renderer->FX_PopRenderTarget(0);
	*/
}

void BenchmarkRendererSensor::afterSwapBuffers(CCryDeviceWrapper& device, CCryDeviceContextWrapper& context)
{
	/*if (m_currentState != TestRunDone) return;

	   forceFlush(device, context);

	   m_currentState = TestInactive;

	   submitResults();
	   cleanupAfterTestRun();*/
}
void BenchmarkRendererSensor::PreStereoFrameSubmit(CTexture* left, CTexture* right)
{

	if (m_currentState == TestRunning)
	{
		m_currentFrameSample.beforeFrameSubmitTimestamp = m_timer->getTimestamp();
	}

	if (m_flickerFunc)
	{
		//if test is running feed the current relative frame ID. Otherwise just constant 0 to allow for initialization etc.
		uint32_t frameID = m_currentState == TestRunning ? m_currentFrameCount : m_frameFlickerID;
		uint32_t flickerMask;
		m_flickerFunc(frameID, &flickerMask);
		bool leftWhite = (flickerMask & 0x1) != 0;
		bool rightWhite = (flickerMask & 0x2) != 0;

		if (m_currentState == TestRunning)
		{
			m_currentFrameSample.flickerMask = flickerMask;
		}

		applyFlicker(left, right, leftWhite, rightWhite);
		++m_frameFlickerID;
	}

}
void BenchmarkRendererSensor::AfterStereoFrameSubmit()
{
	if (m_currentState != TestRunning) return;
	m_currentFrameSample.afterFrameSubmitTimestamp = m_timer->getTimestamp();
	accumulateFrameCount();
}

void BenchmarkRendererSensor::accumulateFrameCount()
{

	if (m_frameSampleInterval >= 0 && (m_frameSampleInterval == 0 || m_currentFrameCount % m_frameSampleInterval == 0))
	{
		saveFrameSample();
	}

	//		CTexture* matrixViewTex = CTexture::GetByID(m_matrixTexHandle);
	//		m_currentIndex = (m_currentIndex + 1) % (m_columns * m_rows);
	/*if (shouldPresentFrame())
	   {
	   //			PostProcessUtils().CopyTextureToScreen(matrixViewTex);
	   m_renderer->EnableSwapBuffers(true);
	   }
	   else
	   {
	   m_renderer->EnableSwapBuffers(false);
	   }*/

	++m_currentFrameCount;

}

void BenchmarkRendererSensor::saveFrameSample()
{
	if (m_frameSampleFlags & BenchmarkFramework::SampleFrameTime)
	{
		m_samples.Add(m_currentFrameSample);
	}

}

#endif //ENABLE_BENCHMARK_SENSOR
