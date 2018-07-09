// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

-------------------------------------------------------------------------
History:
- 15:05:2009   Created by Federico Rebora

*************************************************************************/

#include "Stdafx.h"

#include "ColorGradientManager.h"

#include "EngineFacade/PluggableEngineFacade.h"

#include "Testing/TestUtilities.h"

#include "Testing/MockColorGradingController.h"

#include <CryRenderer/IColorGradingController.h>

CRY_TEST_FIXTURE(ColorGradientManager_TestFixture, CryUnit::ITestFixture, GameTesting::MainTestSuite)
{
private:
	class CMock3DEngine : public EngineFacade::CDummyEngine3DEngine
	{
	public:
		CMock3DEngine(EngineFacade::IEngineColorGradingController& colorGradingController)
		: m_colorGradingController(colorGradingController)
		{

		}

		virtual EngineFacade::IEngineColorGradingController& GetEngineColorGradingController()
		{
			return m_colorGradingController;
		}

	private:
		EngineFacade::IEngineColorGradingController& m_colorGradingController;
	};

public:
	ColorGradientManager_TestFixture()
	: m_mock3DEngine(m_mockColorGradingController)
	, m_colorGradientManager(m_engine)
	{
	}

	void SetUp()
	{
		m_engine.Use(m_mock3DEngine);
	}

protected:
	GameTesting::CMockEngineColorGradingController m_mockColorGradingController;
	CMock3DEngine m_mock3DEngine;
	EngineFacade::CDummyPluggableEngineFacade m_engine;
	Graphics::CColorGradientManager m_colorGradientManager;
};



CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestTriggeredFadingColorGradientLoadsOnUpdate, ColorGradientManager_TestFixture)
{
	const char* const testPath = "testPath";

	m_colorGradientManager.TriggerFadingColorGradient(testPath, 0.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_ARE_EQUAL(testPath, m_mockColorGradingController.GetPathOfLastLoadedColorChart());
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestNothingIsLoadedIfNothingWasTriggered, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_IS_FALSE(m_mockColorGradingController.AnyChartsWereLoaded());
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestMoreGradientsCanBeTriggeredInOneFrame, ColorGradientManager_TestFixture)
{
	const char* const testPath1 = "testPath1";
	m_colorGradientManager.TriggerFadingColorGradient(testPath1, 0.0f);

	const char* const testPath2 = "testPath2";
	m_colorGradientManager.TriggerFadingColorGradient(testPath2, 0.0f);

	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_ARE_EQUAL(testPath1, m_mockColorGradingController.GetPathOfLoadedColorChart(0));
	ASSERT_ARE_EQUAL(testPath2, m_mockColorGradingController.GetPathOfLoadedColorChart(1));
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestGradientsAreLoadedOnlyOnce, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 0.0f);

	m_colorGradientManager.UpdateForThisFrame(0.0f);
	m_mockColorGradingController.ClearLoadedCharts();
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_IS_FALSE(m_mockColorGradingController.AnyChartsWereLoaded());
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestGradientWithZeroFadingTimeIsUsedImmediatelyWithFullWeight, ColorGradientManager_TestFixture)
{
	const int expectedTexID = 64;
	m_mockColorGradingController.SetFakeIDForLoadedTexture(expectedTexID);

	m_colorGradientManager.TriggerFadingColorGradient("testPath", 0.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	const SColorChartLayer& theLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);
	
	const SColorChartLayer expectedColorChartLayer(expectedTexID, 1.0f);

	ASSERT_ARE_EQUAL(expectedColorChartLayer, theLayerSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestBlendWeightHalfWayThroughFadingIn, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	const SColorChartLayer& theLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);

	const SColorChartLayer expectedColorChartLayer(0, 0.5f);

	ASSERT_ARE_EQUAL(expectedColorChartLayer, theLayerSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestBlendWeightIsOneWhenFadingIsFinished, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 5.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	const SColorChartLayer& theLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);

	const SColorChartLayer expectedColorChartLayer(0, 1.0f);

	ASSERT_ARE_EQUAL(expectedColorChartLayer, theLayerSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestBlendWeightIsOneAfterFadingIsFinished, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 5.0f);
	m_colorGradientManager.UpdateForThisFrame(10.0f);

	const SColorChartLayer& theLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);

	const SColorChartLayer expectedColorChartLayer(0, 1.0f);

	ASSERT_ARE_EQUAL(expectedColorChartLayer, theLayerSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestNullLayersAreNotSet, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_IS_FALSE(m_mockColorGradingController.WasNullPointerSetOnLayers())
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestBlendWeightIsOneAfterFadingIsFinishedInMoreUpdates, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 10.0f);

	m_colorGradientManager.UpdateForThisFrame(2.0f);
	m_colorGradientManager.UpdateForThisFrame(2.0f);
	m_colorGradientManager.UpdateForThisFrame(2.0f);
	m_colorGradientManager.UpdateForThisFrame(2.0f);
	m_colorGradientManager.UpdateForThisFrame(2.0f);

	const SColorChartLayer& theLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);

	const SColorChartLayer expectedColorChartLayer(0, 1.0f);

	ASSERT_ARE_EQUAL(expectedColorChartLayer, theLayerSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestTwoLayersSet, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath1", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(10.0f);

	m_colorGradientManager.TriggerFadingColorGradient("testPath2", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	const unsigned int numberOfLayersSet = (int) m_mockColorGradingController.GetNumberOfLayersSet();
	ASSERT_ARE_EQUAL(2u, numberOfLayersSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestLayerIntroducedWhileAnotherOneWasFadingIn, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath1", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(10.0f);

	const int expectedTexID = 64;
	m_mockColorGradingController.SetFakeIDForLoadedTexture(expectedTexID);

	m_colorGradientManager.TriggerFadingColorGradient("testPath2", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);
	
	const SColorChartLayer expectedFirstColorChartLayer(0, 0.5f);
	ASSERT_ARE_EQUAL(expectedFirstColorChartLayer,  m_mockColorGradingController.GetCurrentlySetLayerByIndex(0));

	const SColorChartLayer expectedSecondColorChartLayer(expectedTexID, 0.5f);
	ASSERT_ARE_EQUAL(expectedSecondColorChartLayer,  m_mockColorGradingController.GetCurrentlySetLayerByIndex(1));
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestNewFadedChartStopsUpdatingPreviousOnes, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	m_colorGradientManager.TriggerFadingColorGradient("", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	m_colorGradientManager.TriggerFadingColorGradient("", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(5.0f);

	const SColorChartLayer& firstLayerSet = m_mockColorGradingController.GetCurrentlySetLayerByIndex(0);

	ASSERT_FLOAT_ARE_EQUAL(0.125f, firstLayerSet.m_blendAmount, 0.001f);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestLayerWithBlendWeightOneSetsOnlyOneLayer, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath1", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	m_colorGradientManager.TriggerFadingColorGradient("testPath2", 0.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	const unsigned int numberOfLayersSet = (int) m_mockColorGradingController.GetNumberOfLayersSet();
	ASSERT_ARE_EQUAL(1u, numberOfLayersSet);
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestLayerWithBlendWeightOneRemovesOtherLayersAndIsSetCorrectly, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath1", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	const int expectedTexID = 64;
	m_mockColorGradingController.SetFakeIDForLoadedTexture(expectedTexID);

	m_colorGradientManager.TriggerFadingColorGradient("testPath2", 0.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	const SColorChartLayer expectedColorChartLayer(expectedTexID, 1.0f);
	ASSERT_ARE_EQUAL(expectedColorChartLayer, m_mockColorGradingController.GetCurrentlySetLayerByIndex(0));
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestRemovedLayerUnloadsTexture, ColorGradientManager_TestFixture)
{
	const int expectedTexID = 64;
	m_mockColorGradingController.SetFakeIDForLoadedTexture(expectedTexID);

	m_colorGradientManager.TriggerFadingColorGradient("testPath1", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	m_colorGradientManager.TriggerFadingColorGradient("testPath2", 0.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_ARE_EQUAL(expectedTexID, m_mockColorGradingController.GetLastUnloadedTextureID());
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestTextureIsNotUnloadedIfInUse, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(10.0f);

	ASSERT_ARE_EQUAL(-1, m_mockColorGradingController.GetLastUnloadedTextureID());
}

CRY_TEST_WITH_FIXTURE(ColorGradientManager_TestTextureIsNotUnloadedIfDeltaIsZero, ColorGradientManager_TestFixture)
{
	m_colorGradientManager.TriggerFadingColorGradient("testPath", 10.0f);
	m_colorGradientManager.UpdateForThisFrame(0.0f);

	ASSERT_ARE_EQUAL(-1, m_mockColorGradingController.GetLastUnloadedTextureID());
}
