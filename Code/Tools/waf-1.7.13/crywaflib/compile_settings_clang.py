# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf

def load_clang_common_settings(v):
	"""
	Setup all compiler/linker flags with are shared over all targets using the clang compiler
	
	!!! But not the actual compiler, since the compiler depends on the target !!!
	"""
	
	# AR Tools
	v['ARFLAGS'] = 'rcs'
	v['AR_TGT_F'] = ''
	
	# CC/CXX Compiler
	v['CC_NAME'] = 'clang'
	v['CC_SRC_F'] = v['CXX_SRC_F'] = []
	v['CC_TGT_F'] = v['CXX_TGT_F'] = ['-c', '-o']
	
	v['CPPPATH_ST'] = '-I%s'
	v['DEFINES_ST'] = '-D%s'
	
	v['ARCH_ST'] = ['-arch']
	
	# Linker
	v['CCLNK_SRC_F'] = v['CXXLNK_SRC_F'] = []
	v['CCLNK_TGT_F'] = v['CXXLNK_TGT_F'] = '-o'
	
	v['LIB_ST'] = '-l%s'
	v['LIBPATH_ST'] = '-L%s'
	v['STLIB_ST'] = '-l%s'
	v['STLIBPATH_ST'] = '-L%s'
	
	# shared library settings
	v['CFLAGS_cshlib'] = v['CFLAGS_cxxshlib'] = ['-fpic']
	v['CXXFLAGS_cshlib'] = v['CXXFLAGS_cxxshlib'] = ['-fpic']
		
	v['LINKFLAGS_cshlib'] = ['-shared', '-Wl,--export-dynamic']
	v['LINKFLAGS_cxxshlib'] = ['-shared', '-Wl,--export-dynamic']
	
	# static library settings
	v['CFLAGS_cstlib'] = v['CFLAGS_cxxstlib'] = ['-fpic']	
	v['CXXFLAGS_cstlib'] = v['CXXFLAGS_cxxstlib'] = ['-fpic']
	
	v['LINKFLAGS_cxxstlib'] = ['-Wl,-Bstatic']
	v['LINKFLAGS_cxxshtib'] = ['-Wl,-Bstatic']
	
	# Set common compiler flags
	COMMON_COMPILER_FLAGS = [
		'-Wall', # Generate more warnings
		'-Werror', # Treat Warnings as Errors
		'-ffast-math', # Enable fast math
		
		'-fvisibility=hidden',
		'-fvisibility-inlines-hidden',
		
		# Disable some warnings
		'-Wno-tautological-compare',
		'-Wno-deprecated-writable-strings',
		'-Wno-unneeded-internal-declaration',
		'-Wno-self-assign',
		'-Wno-c++11-narrowing',
		'-Wno-dynamic-class-memaccess',
		'-Wno-sizeof-pointer-memaccess',
		'-Wno-char-subscripts',
		'-Wno-invalid-offsetof',
		'-Wno-unused-private-field',
		'-Wno-sometimes-uninitialized',
		'-Wno-c++11-extensions',
		'-Wno-unknown-pragmas',
		'-Wno-unused-variable',
		'-Wno-unused-value',
		'-Wno-parentheses',
		'-Wno-switch',
		'-Wno-unused-function',
		'-Wno-unused-result',
		'-Wno-multichar',
		'-Wno-format-security',
		'-Wno-empty-body',
		'-Wno-comment',
		'-Wno-char-subscripts',
		'-Wno-sign-compare',
		'-Wno-narrowing',
		'-Wno-write-strings',
		'-Wno-format',
		'-Wno-null-character',
		'-Wno-reorder',
		'-Wno-conversion-null',
		'-Wno-overloaded-virtual',
		'-Wno-shift-sign-overflow',
		'-Wno-deprecated-register'
		]
		
	# Copy common flags to prevent modifing references
	v['CFLAGS'] += COMMON_COMPILER_FLAGS[:]
	
	
	v['CXXFLAGS'] += COMMON_COMPILER_FLAGS[:] + [
		'-fno-rtti', # Disable RTTI
		'-fno-exceptions', # Disable Exceptions
		'-std=c++11', # Enable c++11 features
	]
	
	# Linker Flags
	v['LINKFLAGS'] += ['-Wl,--rpath=$ORIGIN']
	
	# Doesn't work on Mac? v['SHLIB_MARKER'] = '-Wl,-Bdynamic'
	v['SONAME_ST'] = '-Wl,-h,%s'
	# Doesn't work on Mac? v['STLIB_MARKER'] = '-Wl,-Bstatic'
	
	# Compile options appended if compiler optimization is disabled
	v['COMPILER_FLAGS_DisableOptimization'] = [ '-O0', '-fno-inline' ]
	
	# Compile options appended if debug symbols are generated
	v['COMPILER_FLAGS_DebugSymbols'] = [ '-g' ]
	
	# Linker flags when building with debug symbols
	v['LINKFLAGS_DebugSymbols'] = []
	
	# Store settings for show includes option
	v['SHOWINCLUDES_cflags'] = ['-H']
	v['SHOWINCLUDES_cxxflags'] = ['-H']
	
	# Store settings for preprocess to file option
	v['PREPROCESS_cflags'] = ['-E', '-dD']
	v['PREPROCESS_cxxflags'] = ['-E', '-dD']
	v['PREPROCESS_cc_tgt_f'] = ['-o']
	v['PREPROCESS_cxx_tgt_f'] = ['-o']
	
	# Store settings for preprocess to file option
	v['DISASSEMBLY_cflags'] = ['-S', '-fverbose-asm']
	v['DISASSEMBLY_cxxflags'] = ['-S', '-fverbose-asm']
	v['DISASSEMBLY_cc_tgt_f'] = ['-o']
	v['DISASSEMBLY_cxx_tgt_f'] = ['-o']
	
	# Store setting for static code analyzer
	always_disabled_flags = ['-Wno-unknown-pragmas']
	disabled_flags = [
		  '-Wno-unused-variable'
		, '-Wno-unused-value'
		, '-Wno-unused-function'
		, '-Wno-unused-private-field'		
		, '-Wno-multichar'
		, '-Wno-reorder'
		, '-Wno-parentheses'
		, '-Wno-switch'
		, '-Wno-bitwise-op-parentheses'
		, '-Wno-overloaded-virtual'
		, '-Wno-comment'
		, '-Wno-sometimes-uninitialized'
		#  '-Wno-subscripts',
		#, '-Wno-address'
		#, '-Wno-self-assign'
		#, '-Wno-unneeded-internal-declaration'
		]
		
	force_enable_flags = ['-Wdeprecated-writable-strings', '-Wtautological-compare']
	
	analyzer_flags = ['-Wall'] + always_disabled_flags + disabled_flags + force_enable_flags
	v['STATIC_CODE_ANALYZE_cflags'] = list(analyzer_flags)
	v['STATIC_CODE_ANALYZE_cxxflags'] = list(analyzer_flags)
	
@conf
def load_debug_clang_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the clang compiler
	for the "debug" configuration
	"""
	v = conf.env
	load_clang_common_settings(v)
	
	COMPILER_FLAGS = [
		'-O0', # No optimization
		]
	
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
@conf
def load_profile_clang_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the clang compiler
	for the "profile" configuration
	"""
	v = conf.env
	load_clang_common_settings(v)
	
	COMPILER_FLAGS = [
		'-O2',
		]
	
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
@conf
def load_performance_clang_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the clang compiler
	for the "performance" configuration
	"""
	v = conf.env
	load_clang_common_settings(v)
	
	COMPILER_FLAGS = [
		'-O3',
		'-fno-stack-protector',
		]
	
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
	
@conf
def load_release_clang_settings(conf):
	"""
	Setup all compiler/linker flags with are shared over all targets using the clang compiler
	for the "release" configuration
	"""
	v = conf.env
	load_clang_common_settings(v)
	
	COMPILER_FLAGS = [
		'-O3',
		'-fno-stack-protector',
		]
	
	v['CFLAGS'] += COMPILER_FLAGS
	v['CXXFLAGS'] += COMPILER_FLAGS
