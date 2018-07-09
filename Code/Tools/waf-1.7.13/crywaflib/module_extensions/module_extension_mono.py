# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

import os
from waflib import Logs
from waflib.TaskGen import feature
from waflib.CryModuleExtension import module_extension
from waflib.Utils import run_once

@module_extension('mono')
def module_extensions_mono(ctx, kw, entry_prefix, platform, configuration):
	
	if not platform.startswith('win'):
		return		
	
	kw[entry_prefix + 'defines'] += [ 'USE_MONO_BRIDGE' ]
	kw[entry_prefix + 'includes'] += [ ctx.CreateRootRelativePath('Code/SDKs/Mono/include/mono-2.0') ]

	if platform == 'win_x86':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Mono/lib/x86') ]
	elif platform == 'win_x64':
		kw[entry_prefix + 'libpath'] += [ ctx.CreateRootRelativePath('Code/SDKs/Mono/lib/x64') ]
		