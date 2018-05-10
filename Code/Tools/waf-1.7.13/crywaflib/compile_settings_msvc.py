# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf

@conf
def load_msvc_common_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the microsoft compiler
	
	!!! But not the actual compiler, since the compiler depends on the target !!!
	"""	
	v = conf.env	
	
	# MT Tool
	v['MTFLAGS'] = ['/nologo']
	
	# AR Tools
	v['ARFLAGS'] += ['/NOLOGO']
	v['AR_TGT_F'] = '/OUT:'
	
	# CC/CXX Compiler
	v['CC_NAME']	= v['CXX_NAME'] 	= 'msvc'
	v['CC_SRC_F']  	= v['CXX_SRC_F']	= []
	v['CC_TGT_F'] 	= v['CXX_TGT_F']	= ['/c', '/Fo']
	
	v['CPPPATH_ST'] 	= '/I%s'
	v['DEFINES_ST'] 	= '/D%s'
	
	v['PCH_FILE_ST'] 	= '/Fp%s'
	v['PCH_CREATE_ST'] 	= '/Yc%s'
	v['PCH_USE_ST'] 	= '/Yu%s'
	
	v['ARCH_ST'] 		= ['/arch:']
	
	# Linker
	v['CCLNK_SRC_F'] = v['CXXLNK_SRC_F'] = []
	v['CCLNK_TGT_F'] = v['CXXLNK_TGT_F'] = '/OUT:'
	
	v['LIB_ST'] 		= '%s.lib'
	v['LIBPATH_ST'] 	= '/LIBPATH:%s'
	v['STLIB_ST'] 		= '%s.lib'
	v['STLIBPATH_ST'] 	= '/LIBPATH:%s'
	
	v['cprogram_PATTERN'] 	= '%s.exe'
	v['cxxprogram_PATTERN'] = '%s.exe'		
	
	# shared library settings	
	v['CFLAGS_cshlib'] = v['CFLAGS_cxxshlib']		= []
	v['CXXFLAGS_cshlib'] = v['CXXFLAGS_cxxshlib']	= []
	
	v['LINKFLAGS_cshlib'] 	= ['/DLL']
	v['LINKFLAGS_cxxshlib'] = ['/DLL']
	
	v['cshlib_PATTERN'] 	= '%s.dll'
	v['cxxshlib_PATTERN'] 	= '%s.dll'
	
	# static library settings	
	v['CFLAGS_cstlib'] = v['CFLAGS_cxxstlib'] = []
	v['CXXFLAGS_cstlib'] = v['CXXFLAGS_cxxstlib'] = []
	
	v['LINKFLAGS_cxxstlib']	= []
	v['LINKFLAGS_cxxshtib'] = []
	
	v['cstlib_PATTERN']      = '%s.lib'
	v['cxxstlib_PATTERN']    = '%s.lib'
	
	v['STATIC_CODE_ANALYZE_cflags'] = ['/analyze']
	v['STATIC_CODE_ANALYZE_cxxflags'] = ['/analyze']
	
	# Set common compiler flags	
	COMMON_COMPILER_FLAGS = [
		'/nologo',		# Suppress Copyright and version number message
		'/W3',			# Warning Level 3
		'/WX',			# Treat Warnings as Errors
		'/MP',			# Allow Multicore compilation
		'/Gy',			# Enable Function-Level Linking		
		'/GF',			# Enable string pooling
		'/Gm-',			# Disable minimal rebuild (causes issues with IB)
		'/fp:fast',		# Use fast (but not as precisce floatin point model)	
		'/Zc:wchar_t',	# Use compiler native wchar_t				
		'/Zc:forScope',	# Force Conformance in for Loop Scope
		'/GR-',			# Disable RTTI		
		'/Gd',			# Use _cdecl calling convention for all functions
		'/utf-8',		# Set source and execution character sets to UTF-8.
		'/Wv:18',		# Disable warnings until SDK depedencies switch to UTF-8/ASCII.
		'/bigobj'       # Enable big object files
		]
		
	# Copy common flags to prevent modifing references
	v['CFLAGS'] += COMMON_COMPILER_FLAGS[:]
	v['CXXFLAGS'] += COMMON_COMPILER_FLAGS[:]
	
	# Linker Flags
	v['LINKFLAGS'] += [
		'/NOLOGO', 				# Suppress Copyright and version number message		
		'/LARGEADDRESSAWARE',	# tell the linker that the application can handle addresses larger than 2 gigabytes.
		]	
		
	# Compile options appended if compiler optimization is disabled
	v['COMPILER_FLAGS_DisableOptimization'] = [ 
		'/Od',  # Turn of all optimization
		'/Ob0' 	# Disables inline expansions
		]	
	
	# Compile options appended if debug symbols are generated
	# Create a external Program Data Base (PDB) for debugging symbols
	v['COMPILER_FLAGS_DebugSymbols'] = [ 
	'/Zi', 	# Produces a program database (PDB)
	'/Zo' 	# Generate enhanced debugging information (Note: Will be converted to /d2Zi+ for msvc 11.0)
	]	
	
	# Linker flags when building with debug symbols
	v['LINKFLAGS_DebugSymbols'] = [ '/DEBUG' ]
	
	# Store settings for show includes option
	v['SHOWINCLUDES_cflags'] = ['/showIncludes']
	v['SHOWINCLUDES_cxxflags'] = ['/showIncludes']
	
	# Store settings for preprocess to file option
	v['PREPROCESS_cflags'] = ['/P', '/C']
	v['PREPROCESS_cxxflags'] = ['/P', '/C']
	v['PREPROCESS_cc_tgt_f'] = ['/c', '/Fi']
	v['PREPROCESS_cxx_tgt_f'] = ['/c', '/Fi']
	
	# Store settings for preprocess to file option
	v['DISASSEMBLY_cflags'] = ['/FAcs']
	v['DISASSEMBLY_cxxflags'] = ['/FAcs']
	v['DISASSEMBLY_cc_tgt_f'] = [ '/c', '/Fa']
	v['DISASSEMBLY_cxx_tgt_f'] = ['/c', '/Fa']

	# Values that are propagates from consumer to producers via "use_module"
	# Basically, flags that affect code-gen in a non-linkable way (ie, runtime type) and defines that change type ABI must be forwarded
	runtime_switches = [ '/MT', '/MTd', '/MD', '/MDd' ]
	v['module_whitelist_cflags'] = v['module_whitelist_cxxflags'] = runtime_switches + v['COMPILER_FLAGS_DebugSymbols'] + v['COMPILER_FLAGS_DisableOptimization']
	v['module_whitelist_defines'] = [ '_MT', '_DLL', '_DEBUG', 'NDEBUG', 'NOT_USE_CRY_MEMORY_MANAGER', 'NOT_USE_CRY_STRING', 'RESOURCE_COMPILER', 'FORCE_STANDARD_ASSERT' ]
	
	
def apply_incremental_linking(conf):
	if conf.is_option_true('use_incremental_linking'):
		conf.env['LINKFLAGS'] += [
			'/INCREMENTAL',  # Enable Incremental Linking
			]
	else:
		conf.env['LINKFLAGS'] += [
			'/OPT:REF',	# Eliminate not referenced functions/data (incompatible with incremental linking)
			'/OPT:ICF',	# Perform Comdat folding (incompatible with incremental linking)
			'/INCREMENTAL:NO',  # Disable Incremental Linking
			]

@conf
def load_debug_msvc_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the microsoft compiler
	for the "debug" configuration	
	"""
	v = conf.env	
	conf.load_msvc_common_settings()
	
	COMPILER_FLAGS = [
		'/Od',		# Disable all optimizations
		'/Ob0',		# Disable all inling
		'/Oy-',		# Don't omit the frame pointer
		'/RTC1',	# Add basic Runtime Checks
		'/GS',		# Use Buffer Security checks
		'/bigobj',	# Ensure we can store enough objects files
		]
		
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
	apply_incremental_linking(conf)
		
@conf
def load_profile_msvc_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the microsoft compiler
	for the "profile" configuration	
	"""
	v = conf.env
	conf.load_msvc_common_settings()
	
	COMPILER_FLAGS = [
		'/Ox',		# Full optimization
		'/Ob2',		# Inline any suitable function
		'/Ot',		# Favor fast code over small code
		'/Oi',		# Use Intrinsic Functions
		'/Oy-',		# Don't omit the frame pointer		
		'/GS-',		# Don't Use Buffer Security checks
		]
		
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
	apply_incremental_linking(conf)
		
@conf
def load_performance_msvc_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the microsoft compiler
	for the "performance" configuration	
	"""
	v = conf.env
	conf.load_msvc_common_settings()
	
	COMPILER_FLAGS = [
		'/Ox',		# Full optimization
		'/Ob2',		# Inline any suitable function
		'/Ot',		# Favor fast code over small code
		'/Oi',		# Use Intrinsic Functions
		'/Oy',		# omit the frame pointer		
		'/GS-',		# Don't Use Buffer Security checks
		]
		
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
	apply_incremental_linking(conf)
		
@conf
def load_release_msvc_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the microsoft compiler
	for the "release" configuration	
	"""
	v = conf.env
	conf.load_msvc_common_settings()
	
	COMPILER_FLAGS = [
		'/Ox',		# Full optimization
		'/Ob2',		# Inline any suitable function
		'/Ot',		# Favor fast code over small code
		'/Oi',		# Use Intrinsic Functions
		'/Oy',		# omit the frame pointer		
		'/GS-',		# Don't Use Buffer Security checks
		]
		
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
	v['LINKFLAGS'] += [
		'/OPT:REF',	# Eliminate not referenced functions/data (incompatible with incremental linking)
		'/OPT:ICF',	# Perform Comdat folding (incompatible with incremental linking)
		'/INCREMENTAL:NO',  # Disable Incremental Linking
		]	
		
