# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs

@conf
def check_darwin_x64_darwin_x64_installed(conf):
	"""
	Check compiler is actually installed on executing machine
	"""
	return  True		
	
@conf
def load_darwin_x64_darwin_x64_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin_x64_darwin_x64 configurations
	"""
	pass
	
@conf
def load_debug_darwin_x64_darwin_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin_x64_darwin_x64 configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_darwin_x64_darwin_x64_common_settings()
	
	# Load addional shared settings
	conf.load_debug_cryengine_settings()
	conf.load_debug_clang_settings()
	conf.load_debug_darwin_settings()	
	
@conf
def load_profile_darwin_x64_darwin_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin_x64_darwin_x64 configurations for
	the 'profile' configuration
	"""
	v = conf.env
	conf.load_darwin_x64_darwin_x64_common_settings()
	
	# Load addional shared settings
	conf.load_profile_cryengine_settings()
	conf.load_profile_clang_settings()
	conf.load_profile_darwin_settings()
	
@conf
def load_performance_darwin_x64_darwin_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin_x64_darwin_x64 configurations for
	the 'performance' configuration
	"""
	v = conf.env
	conf.load_darwin_x64_darwin_x64_common_settings()
	
	# Load addional shared settings
	conf.load_performance_cryengine_settings()
	conf.load_performance_clang_settings()
	conf.load_performance_darwin_settings()
	
@conf
def load_release_darwin_x64_darwin_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin_x64_darwin_x64 configurations for
	the 'release' configuration
	"""
	v = conf.env
	conf.load_darwin_x64_darwin_x64_common_settings()
	
	# Load addional shared settings
	conf.load_release_cryengine_settings()
	conf.load_release_clang_settings()
	conf.load_release_darwin_settings()
	