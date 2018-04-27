# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

import unittest

class TestMiscFunctions(unittest.TestCase):
    def setUp(self):
        general.open_level_no_prompt("gamesdk/Levels/_TestMaps/Sandbox_Tests/Sandbox_Tests.cry")        

    def test_add_entity_link(self):
        entity.add_entity_link("AnimObject1", "AnimObject2", "TestLink")
        
unittest.main(exit = False, verbosity = 2, defaultTest="TestMiscFunctions")