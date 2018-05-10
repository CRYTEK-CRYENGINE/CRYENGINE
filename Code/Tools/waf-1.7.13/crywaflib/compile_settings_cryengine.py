# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
	
from waflib.Configure import conf
import os

@conf
def init_compiler_settings(conf):
	v = conf.env
	# Create empty env values to ensure appending always works
	v['DEFINES'] = []
	v['INCLUDES'] = []
	v['CXXFLAGS'] = []
	v['LIB'] = []
	v['LIBPATH'] = []
	v['LINKFLAGS'] = []
	v['BINDIR'] = ''
	v['LIBDIR'] = ''
	v['PREFIX'] = ''

@conf
def load_cryengine_common_settings(conf):
	"""
	Setup all platform, compiler and configuration agnostic settings
	"""
	v = conf.env

	# Generate CODE_BASE_FOLDER define to allow to create absolute paths in source to use for pragma comment lib 
	code_node = conf.srcnode.make_node('Code')
	code_path = code_node.abspath()
	code_path = code_path.replace('\\', '/')
	v['DEFINES'] += [ 'CODE_BASE_FOLDER="' + code_path + '/"' ]
	
	# To allow pragma comment (lib, 'SDKs/...) uniformly, pass Code to the libpath
	v['LIBPATH'] += [ conf.CreateRootRelativePath('Code') ]
	
@conf	
def load_debug_cryengine_settings(conf):
	"""
	Setup all platform, compiler agnostic settings for the debug configuration
	"""
	v = conf.env
	conf.load_cryengine_common_settings()
	
	v['DEFINES'] += [ '_DEBUG' ]
		
@conf	
def load_profile_cryengine_settings(conf):
	"""
	Setup all platform, compiler agnostic settings for the profile configuration
	"""
	v = conf.env
	conf.load_cryengine_common_settings()
	
	v['DEFINES'] += [ '_PROFILE', 'PROFILE' ]
		
@conf	
def load_performance_cryengine_settings(conf):
	"""
	Setup all platform, compiler agnostic settings for the performance configuration
	"""
	v = conf.env
	conf.load_cryengine_common_settings()
	
	v['DEFINES'] += [ '_RELEASE', 'PERFORMANCE_BUILD' ]
	
@conf	
def load_release_cryengine_settings(conf):
	"""
	Setup all platform, compiler agnostic settings for the release configuration
	"""	
	v = conf.env
	conf.load_cryengine_common_settings()
	
	v['DEFINES'] += [ '_RELEASE' ]
	
#############################################################################	
#############################################################################

	
#############################################################################
@conf	
def set_editor_module_flags(self, kw):

	kw['includes'] = ['.'] + kw['includes'] # ensure module folder is included first
	kw['includes'] += [
		self.CreateRootRelativePath('Code/Sandbox/EditorInterface'),
		self.CreateRootRelativePath('Code/Sandbox/Plugins/EditorCommon'),
		self.CreateRootRelativePath('Code/CryEngine/CryCommon') ,
        self.CreateRootRelativePath('Code/CryEngine/CryCommon/3rdParty') ,
		self.CreateRootRelativePath('Code/SDKs/boost')
		]
	
	if 'priority_includes' in kw:
		kw['includes'] = kw['priority_includes'] + kw['includes']
	
	kw['defines'] += [
		'WIN32',
		'CRY_ENABLE_RC_HELPER',
		'_AFXDLL', # For MFC see MSDN: "Regular DLLs Dynamically Linked to MFC"
		'IS_EDITOR_BUILD',
		'QT_FORCE_ASSERT '
		]
	
	kw['features'] += ['qt']
	kw['module_extensions'] += [ 'python27' ]

	kw['use'] += ['CryQt']
	kw['includes'] += [self.CreateRootRelativePath('Code/Sandbox/Libs/CryQt')]
	
#############################################################################
@conf	
def set_editor_flags(self, kw):

	set_editor_module_flags(self, kw)
	
	kw['includes'] += [
	self.CreateRootRelativePath('Code/Sandbox/EditorQt'),
	self.CreateRootRelativePath('Code/Sandbox/EditorQt/Include')
	]
	
	kw['defines'] += ['SANDBOX_EDITOR_IMPL'] #HACK temporary to achieve GetIEditor() returning IEditorImpl for sandbox only

###############################################################################
@conf	
def set_rc_flags(self, kw):

	kw['includes'] = [
		'.',
		self.CreateRootRelativePath('Code/CryEngine/CryCommon'),
        self.CreateRootRelativePath('Code/CryEngine/CryCommon/3rdParty') ,
		self.CreateRootRelativePath('Code/SDKs/boost'),
		self.CreateRootRelativePath('Code/Sandbox/Plugins/EditorCommon'),
	] + kw['includes']

	kw['defines'] += [
			'WIN32',
			'RESOURCE_COMPILER',
			'FORCE_STANDARD_ASSERT',
			'NOT_USE_CRY_MEMORY_MANAGER',
			]

###############################################################################
@conf	
def set_pipeline_flags(self, kw):

	kw['includes'] = [
		'.',
		self.CreateRootRelativePath('Code/CryEngine/CryCommon'),
        self.CreateRootRelativePath('Code/CryEngine/CryCommon/3rdParty') 
	] + kw['includes']

	kw['defines'] += [
			'WIN32',
			'RESOURCE_COMPILER',
			'FORCE_STANDARD_ASSERT',
			'NOT_USE_CRY_MEMORY_MANAGER',
			]

###############################################################################
@conf
def CreateRootRelativePath(self, path):	
	"""
	Generate a path relative from the root
	"""
	result_path = self.srcnode.make_node(path)
	return result_path.abspath()
	
###############################################################################
@conf
def SettingsWrapper_Impl(self, *k, **kw):
	
	if not kw.get('files', None) and not kw.get('file_list', None) and not kw.get('regex', None):
		self.fatal("A Settings container must provide a list of verbatim file names, a waf_files file list or an regex")
		
	return kw
