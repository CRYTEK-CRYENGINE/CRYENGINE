# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Configure import conf
from waflib.TaskGen import after_method, feature

@feature('cxxprogram', 'cxxshlib', 'cprogram', 'cshlib', 'c', 'cxx')
@after_method('apply_link')
@after_method('process_pch_msvc') # Make sure analyzer flags are also added to pch files
def add_static_code_analyzer_compile_flags(self):	
	if not 'build_' in self.bld.cmd:
		return # Don't add flags if we don't compile
	
	if not self.bld.is_option_true('use_static_code_analyzer'):
		return
		
	if getattr(self, 'exclude_from_static_code_analyzer', False):
		return

	platform = self.bld.env['PLATFORM']
	configuration = self.bld.env['CONFIGURATION']
	if self.env['STATIC_CODE_ANALYZE_cflags'] == []:	
		self.bld.fatal('No static code analyzer flags specified for %s|%s, aborting compilation' % (platform, configuration) )
	
	if self.env['STATIC_CODE_ANALYZE_cxxflags'] == []:	
		self.bld.fatal('No static code analyzer flags specified for %s|%s, aborting compilation' % (platform, configuration) )
	
	cflags = self.env['STATIC_CODE_ANALYZE_cflags']
	if not isinstance(cflags, list):
		cflags = [ cflags ]
	cxxflags = self.env['STATIC_CODE_ANALYZE_cxxflags']
	if not isinstance(cxxflags, list):
		cxxflags = [ cxxflags ]	
		
	for t in getattr(self, 'compiled_tasks', []):
		t.env.detach() # detach environment as its internal members might be shared between other tasks
		t.env['CXXFLAGS'].extend(cxxflags) 		
		t.env['CFLAGS'].extend(cflags) 		
	
	if hasattr(self, 'pch_task'):
		self.pch_task.env.detach() # detach environment as its internal members might be shared between other tasks
		self.pch_task.env['CXXFLAGS'].extend(cxxflags) 		
		self.pch_task.env['CFLAGS'].extend(cflags) 		
		
		