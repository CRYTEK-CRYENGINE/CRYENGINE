// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef GAME_TESTING_EDITOR_SMOCK_TEST_FIXTURE_H
#define GAME_TESTING_EDITOR_SMOCK_TEST_FIXTURE_H

#include "BinariesPathHelper.h"

struct ISystem;
class CEditorGame;

namespace GameTesting
{
	CRY_TEST_FIXTURE(EditorSmokeTestFixture, CryUnit::ITestFixture, EditorSmokeTestSuite)
    {
    public:
        void SetUp();
        void TearDown();

    private:
        void CreateSystem(SSystemInitParams& initParams);
        HMODULE LoadSystemLibrary(const SSystemInitParams& systemInit);
        static ISystem* g_system;

    protected:
		BinariesPathHelper m_pathHelper;
        static CEditorGame* g_editorGame;
    };

}

#endif
