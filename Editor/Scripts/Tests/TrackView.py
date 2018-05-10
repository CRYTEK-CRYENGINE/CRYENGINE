# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

import unittest

class TestTrackViewFunctions(unittest.TestCase):
    def setUp(self):
        general.open_level_no_prompt("gamesdk/Levels/_TestMaps/Sandbox_Tests/Sandbox_Tests.cry")
        trackview.set_recording(False)

    def test_base_anim_position(self):
        object.set_position("AnimObject1", 1030, 1030, 0) 

        trackview.set_current_sequence("TestSequence")
        trackview.set_time(10)
        self.assertEqual(object.get_position("AnimObject1"), (1024, 1031, 0))
        object.set_position("AnimObject1", 1000, 1000, 0)
        
        trackview.set_current_sequence("")
        self.assertEqual(object.get_position("AnimObject1"), (1030, 1030, 0))        

        trackview.set_current_sequence("TestSequence")
        trackview.set_time(10)
        self.assertEqual(object.get_position("AnimObject1"), (1000, 1000, 0))        
        
    def test_recording(self):
        trackview.set_current_sequence("TestSequence")
        trackview.set_time(5)
        trackview.set_recording(True)
        
        # Recording only works on selected objects. Those should NOT add a new key to the track or move the object, because the object is not selected.        
        oldPosition = object.get_position("AnimObject1")
        object.set_position("AnimObject1", 0, 0, 0)
        self.assertEqual(object.get_position("AnimObject1"), oldPosition)
        
        oldScale = object.get_scale("AnimObject1")
        object.set_scale("AnimObject1", 2, 2, 2)
        self.assertEqual(object.get_scale("AnimObject1"), oldScale)
        
        oldRotation = object.get_rotation("AnimObject1")        
        object.set_rotation("AnimObject1", 30, 30, 30)
        self.assertEqual(object.get_rotation("AnimObject1"), oldRotation)
        
        # Now select the object and do that again
        selection.select_object("AnimObject1")          
        object.set_position("AnimObject1", 1030, 1030, 0)
        self.assertEqual(object.get_position("AnimObject1"), (1030, 1030, 0))
        
        object.set_scale("AnimObject1", 2, 2, 2)
        self.assertEqual(object.get_scale("AnimObject1"), (2, 2, 2))
        
        object.set_rotation("AnimObject1", 30, 30, 30)
        newRotation = tuple([round(x, 3) for x in object.get_rotation("AnimObject1")])
        self.assertEqual(newRotation, (30, 30, 30))
        
        # This should NOT add a new key to the track, because the time is still the same, but the object should still move.
        object.set_position("AnimObject1", 1025, 1027, 0)
        self.assertEqual(object.get_position("AnimObject1"), (1025, 1027, 0))
        
        # Changing time should NOT add new keys
        trackview.set_time(3)

        # Check resulting key count
        self.assertEqual(trackview.get_num_track_keys("position", 0, "AnimObject1", ""), 3)
        self.assertEqual(trackview.get_num_track_keys("rotation", 0, "AnimObject1", ""), 2)
        self.assertEqual(trackview.get_num_track_keys("scale", 0, "AnimObject1", ""), 2)
        
        trackview.set_recording(False)
        
    def test_add_entities(self):
        trackview.set_current_sequence("TestSequence")
        self.assertEqual(trackview.get_num_nodes(""), 1)
    
        selection.clear()    
        selection.select_objects(["AnimObject1", "AnimObject2"])
        trackview.add_selected_entities()
        
        self.assertEqual(trackview.get_num_track_keys("position", 0, "AnimObject2", ""), 0)
        self.assertEqual(trackview.get_num_nodes(""), 2)
        
    def test_move_without_recording(self):
        trackview.set_current_sequence("TestSequence")
        object.set_position("AnimObject1", 1030, 1030, 0)
        self.assertEqual(trackview.get_num_track_keys("position", 0, "AnimObject1", ""), 2)
        self.assertEqual(trackview.get_key_value("position", 0, 0, "AnimObject1", ""), (1030, 1030, 0))  
        self.assertEqual(trackview.get_key_value("position", 0, 1, "AnimObject1", ""), (1030, 1037, 0))
        
    def test_undo_redo(self):
        trackview.new_sequence("TestSequence2")
        self.assertEqual(trackview.get_num_sequences(), 2)
        
        trackview.set_sequence_time_range("TestSequence2", 0, 20)
        self.assertEqual(trackview.get_sequence_time_range("TestSequence2"), (0, 20))
        
        trackview.set_current_sequence("TestSequence2")
        selection.select_objects(["AnimObject1", "AnimObject2"])
        trackview.add_selected_entities()
        
        self.assertEqual(trackview.get_num_nodes(""), 2)
        
        general.undo()
        general.undo()
        self.assertEqual(trackview.get_num_nodes(""), 0)        
        
        general.undo()
        self.assertEqual(trackview.get_sequence_time_range("TestSequence2"), (0, 10))
        
        general.undo()
        self.assertEqual(trackview.get_num_sequences(), 1)
        self.assertEqual(trackview.get_sequence_name(0), "TestSequence")
        
        general.redo()
        general.redo()
        general.redo()
        general.redo()
        
        self.assertEqual(trackview.get_sequence_time_range("TestSequence2"), (0, 20))
        self.assertEqual(trackview.get_num_sequences(), 2)
        trackview.set_current_sequence("TestSequence2")
        self.assertEqual(trackview.get_num_nodes(""), 2)
        
    def test_object_rename(self):
        object.rename_object("AnimObject1", "AnimObject3")
        trackview.set_current_sequence("TestSequence")
        nodeName = trackview.get_node_name(0, "")
        self.assertEqual(nodeName, "AnimObject3")
        
unittest.main(exit = False, verbosity = 2, defaultTest="TestTrackViewFunctions")