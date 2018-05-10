# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib import Logs
from xml.dom.minidom import parse
from waflib.TaskGen import after_method, feature
import os
from waflib import Utils

try:
	import _winreg as winreg
except ImportError:
	try:
		import winreg
	except ImportError:
		winreg = None
		
###############################################################################
# Copy Pasted from MSVS
def convert_waf_platform_to_vs_platform(platform):
	if platform == 'win_x86':
		return 'Win32'
	if platform == 'win_x64':
		return 'x64'
	if platform == 'durango':
		return 'Durango'
	if platform == 'orbis':
		return 'ORBIS'
	print('to_vs error' + platform)
	return 'UNKNOWN'
	
###############################################################################
# Copy Pasted from MSVS	
def convert_waf_configuration_to_vs_configuration(configuration):
	configuration = configuration.replace('debug', 		'Debug')
	configuration = configuration.replace('profile',	'Profile')
	configuration = configuration.replace('release', 	'Release')
	configuration = configuration.replace('performance', 	'Performance')
	return configuration
	
###############################################################################
@conf
def check_global_recode_enabled(bld):

	if not winreg:
		return False

	###########################################
	# Only modify env when compiling on a platform supporting recode
	if  bld.env['PLATFORM'] != 'win_x86' and  bld.env['PLATFORM'] != 'win_x64':
		return False
		
	###########################################
	# Check global recode setting
	if not bld.is_option_true('support_recode'):
		return False

	###########################################
	# Open Registry Key to check recode install directory			
	try:
		recode_settings = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Indefiant\\Recode", 0, winreg.KEY_READ)	
		recode_install_path = winreg.QueryValueEx(recode_settings, 'InstallDir')[0]
	except:			
		return False
	
	if recode_install_path == '':
		return False
		
	bld.recode_install_path = recode_install_path			
				
	# Add path to env to allow recode to find its DLLs
	platform = bld.env['PLATFORM']
	# Make sure we have an env to use
	for config in bld.get_supported_configurations():
		if bld.all_envs[ platform + '_' + config ].env == []:	
			bld.all_envs[ platform + '_' + config ].env = os.environ.copy()
	
	# Figure out path based on platform to append
	if platform == 'win_x86':		
		path = ';' + str(recode_install_path) + 'Win32'
	elif platform == 'win_x64':		
		path = ';' + str(recode_install_path) + 'x64\\'
	
	# Now add path to environment
	for config in bld.get_supported_configurations():
		bld.all_envs[ platform + '_' + config ].env['PATH'] += path

	return True		
		
###############################################################################
@conf
def check_project_recode_enabled(bld, target):
	if not bld.is_option_true('support_recode'):
		return False
	
	if not bld.enable_recode:
		return False
	
	if bld.env['PLATFORM'] == 'project_generator':
		return False
		
	if Utils.unversioned_sys_platform() != 'win32':
		return False
		
	recode_file_node = bld.projects_dir.make_node(target + '.vcxproj.recode.user').get_src()		
	if not os.path.exists(recode_file_node.abspath()):		
		return False
	

	# Get current platform and configuration, take overwrites into account
	platform 		=  bld.env['PLATFORM']
	configuration 	=  bld.env['CONFIGURATION']
	if target in bld.env['CONFIG_OVERWRITES']:
		configuration = bld.env['CONFIG_OVERWRITES'][target]
	
	# parse recode user files
	recode_file = parse(recode_file_node.abspath())

	for node in recode_file.getElementsByTagName('PropertyGroup'):
		if node.getAttribute('Label') == 'Recode':		
			# Check Condition if this is for our current build target
			# Take config overwrites into account
			condition = node.getAttribute('Condition')

			if not convert_waf_platform_to_vs_platform(platform) in condition:
				continue

			if not convert_waf_configuration_to_vs_configuration(configuration) in condition:
				continue

			return False
		

	# User configs only disable recode, so return True if nothing was found
	return True
	
@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib')
@after_method('apply_link')
def add_recode_compile_flags(self):	
	platform =  self.env['PLATFORM']
	if platform != 'win_x86' and platform != 'win_x64':
		return # Recode is only supported on windows targets

	# Check global recode setting
	if not self.bld.is_option_true('support_recode'):
		return
			
	# Patch linker flags if we have a link task
	if getattr(self, 'link_task', None):
		patched_linkflags = []
		for flag in self.link_task.env['LINKFLAGS']:
			if '/INCREMENTAL' in flag: #also /INCREMENTAL:NO, add flag on later again
				continue
			if '/DEBUG' in flag: # just ensure not duplicate,  add flag on later again
				continue
			if '/OPT:REF' in flag:
				continue
			if '/OPT:ICF' in flag:
				continue
			if '/LTCG' in flag:
				continue

			patched_linkflags.append(flag)			
		
		self.link_task.env['LINKFLAGS'] = patched_linkflags + [ '/INCREMENTAL' ] + ['/DEBUG']			
