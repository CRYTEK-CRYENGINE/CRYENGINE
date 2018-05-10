# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib.TaskGen import feature, before_method
from waflib.Utils import run_once
 
@conf
def load_windows_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all windows configurations
	"""
	v = conf.env
	
	# Configure manifest tool
	v.MSVC_MANIFEST = True
	
	# Setup default libraries to always link
	v['LIB'] += [ 'User32', 'Advapi32', 'Ntdll', 'Ws2_32' ]
	
	# Load Resource Compiler Tool
	conf.load_rc_tool()
	
@conf
def load_debug_windows_settings(conf):
	"""
	Setup all compiler and linker settings shared over all windows configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_windows_common_settings()
	
@conf
def load_profile_windows_settings(conf):
	"""
	Setup all compiler and linker settings shared over all windows configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_windows_common_settings()
	
@conf
def load_performance_windows_settings(conf):
	"""
	Setup all compiler and linker settings shared over all windows configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_windows_common_settings()
	
@conf
def load_release_windows_settings(conf):
	"""
	Setup all compiler and linker settings shared over all windows configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_windows_common_settings()

@feature('copy_physx_modules')
@before_method('apply_link')
@run_once
def copy_physx_modules(self):
	if self.env['PLATFORM'] == 'project_generator':
		return

	src_dir = 'Code/SDKs/PhysX/Bin/vc14win64'
	src = self.bld.root.make_node(src_dir)
	dst = self.bld.get_output_folders(self.bld.env['PLATFORM'], self.bld.env['CONFIGURATION'])[0]
	dst.mkdir();

	lib = ['PhysX3_x64.dll','PhysX3CharacterKinematic_x64.dll','PhysX3Common_x64.dll','PhysX3Cooking_x64.dll','PhysX3PROFILE_x64.dll','PhysX3CharacterKinematicPROFILE_x64.dll','PhysX3CommonPROFILE_x64.dll','PhysX3CookingPROFILE_x64.dll','PhysX3DEBUG_x64.dll','PhysX3CharacterKinematicDEBUG_x64.dll','PhysX3CommonDEBUG_x64.dll','PhysX3CookingDEBUG_x64.dll','nvToolsExt64_1.dll']
	
	for file in lib:
		self.create_task('copy_outputs', src.make_node(file), dst.make_node(file))
