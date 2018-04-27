# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs

@conf
def check_linux_x64_linux_x64_gcc_installed(conf):
	"""
	Check compiler is actually installed on executing machine
	"""
	return  True	
	
@conf
def load_linux_x64_linux_x64_gcc_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_gcc configurations
	"""
	v = conf.env
	
	# Setup Tools for GCC Toolchain (simply used system installed version)
	v['AR'] = 'ar'
	v['CC'] = 'gcc-4.9'
	v['CXX'] = 'g++-4.9'
	v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = 'g++-4.9'
	
	# Introduce the compiler to generate 32 bit code
	v['CFLAGS'] += [ '-m64' ]
	v['CXXFLAGS'] += [ '-m64' ]		
	
@conf
def load_debug_linux_x64_linux_x64_gcc_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_gcc configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_gcc_common_settings()
	
	# Load addional shared settings
	conf.load_debug_cryengine_settings()
	conf.load_debug_gcc_settings()
	conf.load_debug_linux_settings()
	conf.load_debug_linux_x64_settings()
	
@conf
def load_profile_linux_x64_linux_x64_gcc_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_gcc configurations for
	the 'profile' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_gcc_common_settings()
	
	# Load addional shared settings
	conf.load_profile_cryengine_settings()
	conf.load_profile_gcc_settings()
	conf.load_profile_linux_settings()
	conf.load_profile_linux_x64_settings()	
	
@conf
def load_performance_linux_x64_linux_x64_gcc_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_gcc configurations for
	the 'performance' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_gcc_common_settings()
	
	# Load addional shared settings
	conf.load_performance_cryengine_settings()
	conf.load_performance_gcc_settings()
	conf.load_performance_linux_settings()
	conf.load_performance_linux_x64_settings()
	
@conf
def load_release_linux_x64_linux_x64_gcc_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x64_linux_x64_gcc configurations for
	the 'release' configuration
	"""
	v = conf.env
	conf.load_linux_x64_linux_x64_gcc_common_settings()
	
	# Load addional shared settings
	conf.load_release_cryengine_settings()
	conf.load_release_gcc_settings()
	conf.load_release_linux_settings()
	conf.load_release_linux_x64_settings()
	
