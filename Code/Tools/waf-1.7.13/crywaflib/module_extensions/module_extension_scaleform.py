# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs, Utils
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension

@Utils.memoize
def check_path_exists(path):
	if os.path.isdir(path):
		return True
	else:
		Logs.warn('[WARNING] ' + path + ' not found, this feature is excluded from this build.')
	return False

@module_extension('scaleform')
def module_extensions_scaleform(ctx, kw, entry_prefix, platform, configuration):
	# If the module extension is pulled in, we want to use the helper even if Scaleform SDK is not available
	kw[entry_prefix + 'defines'] += [ 'CRY_FEATURE_SCALEFORM_HELPER' ]

	# Only include scaleform if it exists
	if not check_path_exists(ctx.CreateRootRelativePath('Code/SDKs/Scaleform')):
		return

	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/Scaleform/Include') ]
	kw[entry_prefix + 'defines'] += [ 'INCLUDE_SCALEFORM_SDK' ]
				
	scaleform_lib_subfolder = 	ctx.CreateRootRelativePath('Code/SDKs/Scaleform/Lib')  + os.sep
	has_shipping_lib = True			
	
	if platform.startswith('win') or platform == 'durango':
		# Add MSVC version
		scaleform_lib_subfolder += 'vc' + str(ctx.env['MSVC_VERSION']).replace('.','') 	+ os.sep
		has_shipping_lib = True
		if platform == 'win_x86':
			scaleform_lib_subfolder += 'Win32'
		elif platform == 'win_x64':
			scaleform_lib_subfolder += 'Win64'
		elif platform == 'durango':
			scaleform_lib_subfolder += 'Durango'
		kw[entry_prefix + 'lib']  += ['libgfx']
	elif platform.startswith('linux'):
		has_shipping_lib = False
		scaleform_lib_subfolder +=  'linux'		
		kw[entry_prefix + 'libpath'] += [scaleform_lib_subfolder]  # add platform lib folder for this platform
		kw[entry_prefix + 'lib']  += ['gfx']
	elif platform == 'orbis':
		has_shipping_lib = True
		scaleform_lib_subfolder +=  'ORBIS'
		kw[entry_prefix + 'lib']  += ['gfx', 'gfx_video']
	elif platform == 'android_arm':
		has_shipping_lib = True
		scaleform_lib_subfolder +=  'android-armeabi-v7a'
		kw[entry_prefix + 'lib']  += ['gfx']
	elif platform == 'android_arm64':
		has_shipping_lib = True
		scaleform_lib_subfolder +=  'android-arm64-v8a'
		kw[entry_prefix + 'lib']  += ['gfx']
	elif platform == 'darwin':
		has_shipping_lib = False
		scaleform_lib_subfolder +=  'mac'
		kw[entry_prefix + 'libpath'] += [scaleform_lib_subfolder]  # add platform lib folder for this platform
		kw[entry_prefix + 'lib']  += ['gfx']

	if configuration == "":	
		configuration = ctx.env['CONFIGURATION']		
		
	if not configuration == 'project_generator':
		scaleform_lib_subfolder += os.sep
		if configuration == 'debug':
			scaleform_lib_subfolder += 'Debug'
		elif configuration == 'profile':
			scaleform_lib_subfolder += 'Release'
		elif configuration == 'performance':
			if has_shipping_lib:			
				scaleform_lib_subfolder += 'Shipping'
				kw[entry_prefix + 'defines'] += ['GFC_BUILD_SHIPPING']
			else:
				scaleform_lib_subfolder += 'Release'
		elif configuration == 'release':
			if has_shipping_lib:			
				scaleform_lib_subfolder += 'Shipping'
				kw[entry_prefix + 'defines'] += ['GFC_BUILD_SHIPPING']
			else:
				scaleform_lib_subfolder += 'Release'
		
		kw[entry_prefix + 'libpath'] += [scaleform_lib_subfolder] # add platform|config lib folder
