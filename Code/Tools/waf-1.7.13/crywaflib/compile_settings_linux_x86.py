# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf

def load_linux_x86_common_settings(v):
	"""
	Setup all compiler and linker settings shared over all linux_x86 configurations
	"""
	
	# Add common linux x86 defines
	v['DEFINES'] += [ 'LINUX32' ]	
	
@conf
def load_debug_linux_x86_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x86 configurations for
	the 'debug' configuration
	"""
	v = conf.env
	load_linux_x86_common_settings(v)
	
@conf
def load_profile_linux_x86_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x86 configurations for
	the 'profile' configuration
	"""
	v = conf.env
	load_linux_x86_common_settings(v)
	
@conf
def load_performance_linux_x86_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x86 configurations for
	the 'performance' configuration
	"""
	v = conf.env
	load_linux_x86_common_settings(v)
	
@conf
def load_release_linux_x86_settings(conf):
	"""
	Setup all compiler and linker settings shared over all linux_x86 configurations for
	the 'release' configuration
	"""
	v = conf.env
	load_linux_x86_common_settings(v)
	