# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf

# To we really need those settings
# COMMON_COMPILER_FLAGS = ['-stdlib=libc++',]
#v['LIB'] = ['c',]

@conf
def load_darwin_common_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin configurations
	"""
	v = conf.env
	
	# Setup common defines for darwin
	v['DEFINES'] += [ 'APPLE', 'MAC', '__APPLE__' ]
	
	# Set Minimum mac os version tp 10.9
	v['CFLAGS'] += [ '-mmacosx-version-min=10.9' ]
	v['CXXFLAGS'] += [ '-mmacosx-version-min=10.9' ]
	v['LINKFLAGS'] += [ '-mmacosx-version-min=10.9', ]
	
	# For now, only support 64 bit MacOs Applications
	v['ARCH'] = ['x86_64']
	
	# Pattern to transform outputs
	v['cprogram_PATTERN'] 	= '%s'
	v['cxxprogram_PATTERN'] = '%s'
	v['cshlib_PATTERN'] 	= 'lib%s.dylib'
	v['cxxshlib_PATTERN'] 	= 'lib%s.dylib'
	v['cstlib_PATTERN']      = 'lib%s.a'
	v['cxxstlib_PATTERN']    = 'lib%s.a'
	v['macbundle_PATTERN']    = 'lib%s.dylib'
	
	# Specify how to translate some common operations for a specific compiler	
	v['FRAMEWORK_ST'] 		= ['-framework']
	v['FRAMEWORKPATH_ST'] 	= '-F%s'
	
	# Default frameworks to always link
	v['FRAMEWORK'] = [ 'Foundation', ]
	
	# Setup compiler and linker settings  for mac bundles
	v['CFLAGS_MACBUNDLE'] = v['CXXFLAGS_MACBUNDLE'] = '-fpic'
	v['LINKFLAGS_MACBUNDLE'] = [
		'-bundle', 
		'-undefined', 
		'dynamic_lookup'
		]
		
	# Temporarely hardcode sysroot, needs to have a more flexible solution
	v['CFLAGS'] += [ '-isysroot/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk', ]
	v['CXXFLAGS'] += [ '-isysroot/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk', ]
	v['LINKFLAGS'] += [ '-isysroot/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk', ]
	
	# Since we only support a single darwin target (clang-64bit), we specify all tools directly here	
	v['AR'] = 'ar'
	v['CC'] = 'clang'
	v['CXX'] = 'clang++'
	v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = 'clang++'
	
@conf
def load_debug_darwin_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_darwin_common_settings()

@conf
def load_profile_darwin_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_darwin_common_settings()

@conf
def load_performance_darwin_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_darwin_common_settings()

@conf
def load_release_darwin_settings(conf):
	"""
	Setup all compiler and linker settings shared over all darwin configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_darwin_common_settings()
	