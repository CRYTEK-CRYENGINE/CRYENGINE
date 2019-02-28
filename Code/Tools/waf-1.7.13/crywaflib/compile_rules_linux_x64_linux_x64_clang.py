# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs

@conf
def check_linux_x64_linux_x64_clang_installed(conf):
	"""
	Check compiler is actually installed on executing machine
	"""
	return  True	
	
@conf
def load_linux_x64_linux_x64_clang_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_clang configurations
	"""
	v = conf.env
	
	# Setup Tools for CLang Toolchain (simply used system installed version)
	v['AR'] = 'ar'
	v['CC'] = 'clang-3.8'
	v['CXX'] = 'clang++-3.8'
	v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = 'clang++-3.8'
	
	# Introduce the compiler to generate 32 bit code
	v['CFLAGS'] += [ '-m64' ]
	v['CXXFLAGS'] += [ '-m64' ]		
	
@conf
def load_debug_linux_x64_linux_x64_clang_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_clang configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_clang_common_settings()
	
	# Load addional shared settings
	conf.load_debug_cryengine_settings()
	conf.load_debug_clang_settings()
	conf.load_debug_linux_settings()
	conf.load_debug_linux_x64_settings()
	
@conf
def load_profile_linux_x64_linux_x64_clang_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_clang configurations for
	the 'profile' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_clang_common_settings()
	
	# Load addional shared settings	
	conf.load_profile_cryengine_settings()
	conf.load_profile_clang_settings()
	conf.load_profile_linux_settings()
	conf.load_profile_linux_x64_settings()	
	
@conf
def load_performance_linux_x64_linux_x64_clang_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_clang configurations for
	the 'performance' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_clang_common_settings()
	
	# Load addional shared settings
	conf.load_performance_cryengine_settings()
	conf.load_performance_clang_settings()
	conf.load_performance_linux_settings()
	conf.load_performance_linux_x64_settings()
	
@conf
def load_release_linux_x64_linux_x64_clang_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_clang configurations for
	the 'release' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_clang_common_settings()
	
	# Load addional shared settings
	conf.load_release_cryengine_settings()
	conf.load_release_clang_settings()
	conf.load_release_linux_settings()
	conf.load_release_linux_x64_settings()
	
