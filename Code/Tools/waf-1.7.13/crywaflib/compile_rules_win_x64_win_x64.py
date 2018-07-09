# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs, Context
import os, re

msvc_target = 'x64'
msvc_arch = 'amd64'

@conf
def check_win_x64_win_x64_installed(conf):
	"""
	Check compiler is actually installed on executing machine
	"""
	# backup env as the calls modify it	
	env_backup = conf.env
	conf.env = conf.env.derive()
		
	conf.load_msvc_compiler(msvc_target, msvc_arch)
	ret_value = True if conf.env['MSVC_VERSION'] else False
			
	# restore environment
	conf.env = env_backup
	return ret_value
	
@conf
def load_win_x64_win_x64_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all win_x64_win_x64 configurations
	"""
	v = conf.env

	# Add defines to indicate a win64 build
	v['DEFINES'] += [ '_WIN32', '_WIN64' ]

	# Introduce the linker to generate 64 bit code
	v['LINKFLAGS'] += [ '/MACHINE:X64' ]
	v['ARFLAGS'] += [ '/MACHINE:X64' ]
	
	conf.load_msvc_compiler(msvc_target, msvc_arch) 
		
@conf
def load_debug_win_x64_win_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all win_x64_win_x64 configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_win_x64_win_x64_common_settings()
	
	# Load addional shared settings
	conf.load_debug_cryengine_settings()
	conf.load_debug_msvc_settings()
	conf.load_debug_windows_settings()	
	
	# Link againt GPA lib for profiling
	v['INCLUDES'] += [ conf.CreateRootRelativePath('Code/SDKs/GPA/include') ]
	
@conf
def load_profile_win_x64_win_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all win_x64_win_x64 configurations for
	the 'profile' configuration
	"""
	v = conf.env
	conf.load_win_x64_win_x64_common_settings()
	
	# Load addional shared settings
	conf.load_profile_cryengine_settings()
	conf.load_profile_msvc_settings()
	conf.load_profile_windows_settings()
	
	# Link againt GPA lib for profiling
	v['INCLUDES'] += [ conf.CreateRootRelativePath('Code/SDKs/GPA/include') ]
	
@conf
def load_performance_win_x64_win_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all win_x64_win_x64 configurations for
	the 'performance' configuration
	"""
	v = conf.env
	conf.load_win_x64_win_x64_common_settings()
	
	# Load addional shared settings
	conf.load_performance_cryengine_settings()
	conf.load_performance_msvc_settings()
	conf.load_performance_windows_settings()
	
	# Link againt GPA lib for profiling
	v['INCLUDES'] += [ conf.CreateRootRelativePath('Code/SDKs/GPA/include') ]
	
@conf
def load_release_win_x64_win_x64_settings(conf):
	"""
	Setup all compiler and linker settings shared over all win_x64_win_x64 configurations for
	the 'release' configuration
	"""
	v = conf.env
	conf.load_win_x64_win_x64_common_settings()
	
	# Load addional shared settings
	conf.load_release_cryengine_settings()
	conf.load_release_msvc_settings()
	conf.load_release_windows_settings()
	
