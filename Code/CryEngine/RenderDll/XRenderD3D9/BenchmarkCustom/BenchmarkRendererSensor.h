// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __Basemark_BenchmarkRendererSensor__
#define __Basemark_BenchmarkRendererSensor__
#pragma once

#ifdef ENABLE_BENCHMARK_SENSOR
	#include <CryRenderer/Tarray.h>
	#include <SimpleCommandQueue.h>
	#include <IBenchmarkRendererSensor.h>
	#include <BenchmarkResults.h>

class CD3D9Renderer;

namespace BenchmarkFramework
{
class IHardwareInfoProvider;
class HighPrecisionTimer;
class IRenderResultsObserver;
}

class BenchmarkRendererSensor : public BenchmarkFramework::IBenchmarkRendererSensor
{
public:
	BenchmarkRendererSensor(CD3D9Renderer* renderer);
	virtual ~BenchmarkRendererSensor();

	//virtual void setMatrixTextureResolution(int w, int h);

	//virtual void setMatrixDimensions(int rows, int columns);

	virtual void startTest() override;
	virtual void endTest() override;
	virtual void setRenderResultsObserver(BenchmarkFramework::IRenderResultsObserver* observer) override;
	virtual void afterSwapBuffers(CCryDeviceWrapper& device, CCryDeviceContextWrapper& context);
	virtual void update() override;
	virtual void PreStereoFrameSubmit(CTexture* left, CTexture* right) override;
	virtual void AfterStereoFrameSubmit() override;
	virtual void enableFlicker(bool enable, BenchmarkFramework::FlickerFunction func) override;

	virtual void setFrameSampling(int interval, int flags) override;
	virtual void overrideMonitorWindowSize(bool override, int width, int height, bool fullscreen);

	virtual void cleanupAfterTestRun();

protected:

	enum FrameMatrixCommand
	{
		StartTest,
		EndTest,
		//			SetMatrixTextureResolution,
		//			SetMatrixDimensions,
		SetFrameSampling,
		ForceWindowSize,
		SetupFlicker
	};

	enum FrameMatrixState
	{
		TestInactive,
		//			TestStarted,
		TestRunning,
		//			TestRunDone

	};
	typedef BenchmarkFramework::SimpleCommandQueue<FrameMatrixCommand, TArray<uint8_t>> CommandQueue;
	CommandQueue                                  m_commandQueue[RT_COMMAND_BUF_COUNT];
	TArray<BenchmarkFramework::RenderFrameSample> m_samples;
	BenchmarkFramework::HighPrecisionTimer*       m_timer;
	BenchmarkFramework::TimeStamp                 m_startedTime;
	BenchmarkFramework::RenderFrameSample         m_currentFrameSample;

	FrameMatrixState                              m_currentState;
	CD3D9Renderer*                                m_renderer;
	BenchmarkFramework::IRenderResultsObserver*   m_resultObserver;
	CCryNameR m_shaderUniformName;
	BenchmarkFramework::FlickerFunction           m_flickerFunc;
	/*		int m_matrixTexHandle;
	    int m_matrixTexWidth;
	    int m_matrixTexHeight;
	    int m_rows;
	    int m_columns;
	    int m_currentIndex;
	 */
	uint32_t m_currentFrameCount;
	uint32_t m_frameFlickerID;

	int      m_frameSampleInterval;
	int      m_frameSampleFlags;

	CFullscreenPass m_copyPass;
	CStretchRectPass m_strechRectPass;

	//		bool m_matrixTexNeedsRefresh;
	//		bool m_matrixTexNeedsClear;
	void         saveFrameSample();

	void         submitResults();

	virtual void initializeTest();
	//		virtual void recreateMatrixTexture();
	//		virtual void clearMatrixTexture();

	virtual bool shouldPresentFrame();

	virtual void processCommands();

	//		void copyFrameToMatrix(CTexture* texture);
	//		void copyStereoFrameToMatrix(CTexture* left, CTexture* right);
	void        copyFrameToScreen(CTexture* texture);
	void        copyStereoFrameToScreen(CTexture* left, CTexture* right);
	void        accumulateFrameCount();

	void        applyFlicker(CTexture* left, CTexture* right, bool leftWhite, bool rightWhite);
	void        blendTextureWithColor(CTexture* tex, float r, float g, float b, float a);

	static void forceFlush(CCryDeviceWrapper& device, CCryDeviceContextWrapper& context);
	bool        isActive();
};

#endif //ENABLE_BENCHMARK_SENSOR

#endif
