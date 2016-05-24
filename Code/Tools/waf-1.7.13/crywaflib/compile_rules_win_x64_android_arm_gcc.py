# Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.
import os, re
from waflib.Configure import conf
from waflib.TaskGen import after_method, before_method, feature, extension
from waflib import Utils
import os.path

android_target_version = 19
android_compiler_version = 4.8
android_app_name = "CRYENGINE SDK"
android_package_name = "com.crytek.cryengine"
android_version_code = "1"
android_version_name = "1.0"
android_debuggable = "true"
android_min_sdk_version = "17"
android_permissions_required = ["WRITE_EXTERNAL_STORAGE", "INTERNET"]

ANT_BUILD_TEMPLATE = r'''<?xml version="1.0" encoding="UTF-8"?>
<project name="AndroidLauncher" default="help">

	<property file="build.properties" />

	<!--REGION-BEGIN: The following properties allow Nsight Tegra to maintain build results per-configuration (debug, release, etc)-->
	<target name="-pre-compile">
		<property name="nsight.tegra.project.all.jars.path" value="${toString:project.all.jars.path}" />
		<path id="project.all.jars.path">
			<path path="${nsight.tegra.project.all.jars.path}" />
			<fileset dir="${jar.libs.dir}">
				<include name="*.jar" />
			</fileset>
		</path>
		<echo>Ant Java version: ${ant.java.version}</echo>
		<echo>Java Tools version: ${java.version}</echo>
	</target>
	<!--REGION-END-->

	<!-- quick check on sdk.dir -->
	<fail	message="sdk.dir is missing. Make sure to generate local.properties using 'android update project' or to inject it through the ANDROID_HOME environment variable."
			unless="sdk.dir" />

	<!-- Import the actual build file.

		To customize existing targets, there are two options:
		1) Customize only one target:
			- copy/paste the target into this file, *before* the <import> task.
			- customize it to your needs.
		2) Customize the whole content of build.xml
			- copy/paste the content of the rules files (minus the top node) into this file, replacing the <import> task.
			- customize to your needs.

		***********************
		****** IMPORTANT ******
		***********************
		In all cases you must update the value of version-tag below to read 'custom' instead of an integer,
		in order to avoid having your file be overridden by tools such as "android update project"
	-->

	<!-- version-tag: 1 -->
	<import file="\\${sdk.dir}/tools/ant/build.xml" />
</project>
'''

APP_MANIFEST_STRING_TEMPLATE = r'''<?xml version="1.0" encoding="utf-8"?>
<resources>
	<string name="app_name">${xml_data['app_name']}</string>
</resources>
'''

APP_MANIFEST_TEMPLATE = r'''<?xml version="1.0" encoding="utf-8"?>
<!-- BEGIN_INCLUDE(manifest) -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
	package="${xml_data['package']}"
	android:versionCode="${xml_data['version_code']}"
	android:debuggable="${xml_data['debuggable']}"
	android:versionName="${xml_data['version_name']}" >

	<!-- This is the platform API where NativeActivity was introduced. -->
	<uses-sdk android:minSdkVersion="${xml_data['min_sdk_version']}" />

	<!-- OpenGL ES 3.0 -->
	<uses-feature android:glEsVersion="0x00030000" android:required="true" /> 

	<!-- Required texture compression support -->
	<supports-gl-texture android:name="GL_IMG_texture_compression_pvrtc" />
	<supports-gl-texture android:name="GL_EXT_texture_compression_s3tc" />

	<!-- Required permissions -->
	${for permission_name in xml_data['premissions_required']}
	<uses-permission android:name= "${"android.permission." + permission_name }" /> 
	${endfor}

	<application android:label="@string/app_name">
		<!-- Our activity loads the generated bootstrapping class in order
			to prelaod any third party shared libraries -->
		<activity android:name="CryEngineActivity"
			android:label="@string/app_name"
			android:theme="@android:style/Theme.NoTitleBar.Fullscreen"
			android:screenOrientation="landscape"
			android:configChanges="orientation|keyboardHidden">

			<!-- Tell NativeActivity the name of our .so -->
			<meta-data android:name="android.app.lib_name" android:value="AndroidLauncher" />
			<intent-filter>
				<action android:name="android.intent.action.MAIN" />
				<category android:name="android.intent.category.LAUNCHER" />
			</intent-filter>
		</activity>
	</application>

</manifest> 
<!-- END_INCLUDE(manifest) -->
'''

COMPILE_TEMPLATE = '''def f(xml_data):
	lst = []
	def xml_escape(value):
		return value.replace("&", "&amp;").replace('"', "&quot;").replace("'", "&apos;").replace("<", "&lt;").replace(">", "&gt;")

	%s

	#f = open('cmd.txt', 'w')
	#f.write(str(lst))
	#f.close()
	return ''.join(lst)
'''

reg_act = re.compile(r"(?P<backslash>\\)|(?P<dollar>\$\$)|(?P<subst>\$\{(?P<code>[^}]*?)\})", re.M)
def compile_template(line):
	"""Compile a template expression into a python function (like jsps, but way shorter)
	"""
	extr = []
	def repl(match):
		g = match.group
		if g('dollar'): return "$"
		elif g('backslash'):
			return "\\"
		elif g('subst'):
			extr.append(g('code'))
			return "<<|@|>>"
		return None

	line2 = reg_act.sub(repl, line)
	params = line2.split('<<|@|>>')
	assert(extr)

	indent = 0
	buf = []
	app = buf.append

	def app(txt):
		buf.append(indent * '\t' + txt)

	for x in range(len(extr)):
		if params[x]:
			app("lst.append(%r)" % params[x])

		f = extr[x]
		if f.startswith('if') or f.startswith('for'):
			app(f + ':')
			indent += 1
		elif f.startswith('py:'):
			app(f[3:])
		elif f.startswith('endif') or f.startswith('endfor'):
			indent -= 1
		elif f.startswith('else') or f.startswith('elif'):
			indent -= 1
			app(f + ':')
			indent += 1
		elif f.startswith('xml:'):
			app('lst.append(xml_escape(%s))' % f[4:])
		else:
			#app('lst.append((%s) or "cannot find %s")' % (f, f))
			app('lst.append(%s)' % f)

	if extr:
		if params[-1]:
			app("lst.append(%r)" % params[-1])
	
	fun = COMPILE_TEMPLATE % "\n\t".join(buf)
	return Task.funex(fun)


re_blank = re.compile('(\n|\r|\\s)*\n', re.M)
def rm_blank_lines(txt):
	txt = re_blank.sub('\r\n', txt)
	return txt


@conf
def check_win_x64_android_arm_gcc_installed(conf):
	"""Check compiler is actually installed on executing machine
	"""
	v = conf.env

	# Setup Tools for GCC Cross Compile Toolchain
	if not conf.is_option_true('auto_detect_compiler'):
		android_sdk_home		= conf.CreateRootRelativePath('Code/SDKs/android-sdk')
		android_ndk_home		= conf.CreateRootRelativePath('Code/SDKs/android-ndk')
		android_java_home		= conf.CreateRootRelativePath('Code/SDKs/jdk')
		android_ant_home 		= conf.CreateRootRelativePath('Code/SDKs/apache-ant')
	else:
		android_sdk_home 		= os.getenv('ANDROID_HOME', "")
		android_ndk_home 		= os.getenv('NDK_ROOT', "")
		android_java_home 	= os.getenv('JAVA_HOME', "")
		android_ant_home 		= os.getenv('ANT_HOME', "")

	# Validate paths
	for path in [android_sdk_home, android_ndk_home, android_java_home, android_ant_home]:
		if not os.path.exists(path):
			conf.cry_warning("Requiered Android SDK path does not exist: %s." % path)
			return False

	# Configure platform and compiler mutations
	platform_target = '/platforms/android-' + str(android_target_version)
	compiler_target = '/arch-arm'
	compiler_version = str(android_compiler_version)
	toolchain = 'arm-linux-androideabi-' + compiler_version

	android_sdk_platform_target = android_sdk_home + platform_target
	android_ndk_platform_compiler_target = android_ndk_home + platform_target + compiler_target
	android_ndk_toolchain_target = android_ndk_home + '/toolchains/' + toolchain + '/prebuilt/windows-x86_64'
	if not os.path.exists(android_ndk_toolchain_target): # Fallback if the 64 bit compiler is not found
		android_ndk_toolchain_target = android_ndk_home + '/toolchains/' + toolchain + '/prebuilt/windows'

	# Validate paths
	for path in [android_sdk_platform_target, android_ndk_platform_compiler_target, android_ndk_toolchain_target]:
		if not os.path.exists(path):
			conf.cry_warning("Requiered Android SDK path does not exist: %s." % path)
			return False

	return  True


@conf
def load_win_x64_android_arm_gcc_common_settings(conf):
	"""Setup all compiler and linker settings shared over all win_x64_arm_linux_androideabi_4_8 configurations
	"""

	v = conf.env

	# Setup Tools for GCC Cross Compile Toolchain
	if not conf.is_option_true('auto_detect_compiler'):	
		android_sdk_home = conf.CreateRootRelativePath('Code/SDKs/android-sdk')
		android_ndk_home = conf.CreateRootRelativePath('Code/SDKs/android-ndk')
		android_java_home = conf.CreateRootRelativePath('Code/SDKs/jdk')
		android_ant_home = conf.CreateRootRelativePath('Code/SDKs/apache-ant')

		# Validate paths
		for path in [android_sdk_home, android_ndk_home, android_java_home, android_ant_home]:
			if not os.path.exists(path):
				conf.cry_error("Requiered Android SDK path does not exist: %s. Fix path or try running WAF with 'auto_detect_compiler' option enabled." % path)
	else:
		android_sdk_home = os.getenv('ANDROID_HOME', "")
		android_ndk_home = os.getenv('NDK_ROOT', "")
		android_java_home = os.getenv('JAVA_HOME', "")
		android_ant_home = os.getenv('ANT_HOME', "")

		# Validate paths
		if not os.path.exists(android_sdk_home):
			conf.cry_error("Unable to locate ANDROID SDK. Environment variable 'ANDROID_HOME' not set? Do you have Tegra Android Development Pack (TADP) installed?")
		if not os.path.exists(android_ndk_home):
			conf.cry_error("Unable to locate ANDROID NDK. Environment variable 'NDK_ROOT' not set? Do you have Tegra Android Development Pack (TADP) installed?")
		if not os.path.exists(android_java_home):
			conf.cry_error("Unable to locate JAVA. Environment variable 'JAVA_HOME' not set? Do you have Tegra Android Development Pack (TADP) installed?")
		if not os.path.exists(android_ant_home):
			conf.cry_error("Unable to locate APACHE ANT. Environment variable 'ANT_HOME' not set? Do you have Tegra Android Development Pack (TADP) installed?")

	# Configure platform and compiler mutations
	platform_target = '/platforms/android-' + str(android_target_version)
	compiler_target = '/arch-arm'
	compiler_version = str(android_compiler_version)
	toolchain = 'arm-linux-androideabi-' + compiler_version

	android_sdk_platform_target = android_sdk_home + platform_target
	android_ndk_platform_compiler_target = android_ndk_home + platform_target + compiler_target
	android_ndk_toolchain_target = android_ndk_home + '/toolchains/' + toolchain + '/prebuilt/windows-x86_64'
	if not os.path.exists(android_ndk_toolchain_target): # Fallback if the 64 bit compiler is not found
		android_ndk_toolchain_target = android_ndk_home + '/toolchains/' + toolchain + '/prebuilt/windows'

	android_stl_home = android_ndk_home + '/sources/cxx-stl/gnu-libstdc++/' + compiler_version

	v['AR'] = android_ndk_toolchain_target + '/bin/arm-linux-androideabi-ar'
	v['CC'] = android_ndk_toolchain_target + '/bin/arm-linux-androideabi-gcc.exe'
	v['CXX'] = android_ndk_toolchain_target + '/bin/arm-linux-androideabi-g++.exe'
	v['LINK'] = v['LINK_CC'] = v['LINK_CXX'] = android_ndk_toolchain_target + '/bin/arm-linux-androideabi-g++.exe'
	v['STRIP'] = android_ndk_toolchain_target + '/bin/arm-linux-androideabi-strip.exe'

	v['JAR'] = android_java_home + '/bin/jar.exe'
	v['JAVAC'] = android_java_home +'/bin/javac.exe'
	v['ANDROID_CLASSPATH'] = android_sdk_platform_target + '/android.jar'
	v['ANDROID_TARGET_VERSION'] = android_target_version

	v['ANT'] = android_ant_home + '/bin/ant.bat'

	v['ANDROID_NDK_HOME'] = android_ndk_home 
	v['ANDROID_SDL_HOME'] = android_stl_home 
	v['ANDROID_SDK_HOME'] = android_sdk_home 
	v['ANDROID_JAVA_HOME'] = android_java_home

	v['cprogram_PATTERN'] 	= '%s'
	v['cxxprogram_PATTERN'] = '%s'
	v['cshlib_PATTERN'] 	= 'lib%s.so'
	v['cxxshlib_PATTERN'] 	= 'lib%s.so'
	v['cstlib_PATTERN']     = 'lib%s.a'
	v['cxxstlib_PATTERN']   = 'lib%s.a'

	v['DEFINES'] += ['_LINUX', 'LINUX', 'LINUX32', 'ANDROID', '_HAS_C9X' ]

	# Setup global include paths
	v['INCLUDES'] += [ 
		android_ndk_platform_compiler_target + '/usr/include', 
		android_stl_home + '/include', 
		android_stl_home + '/libs/armeabi-v7a/include',
		android_ndk_home + '/sources/android/support/include',
		android_ndk_home + '/sources/android/native_app_glue',
		conf.CreateRootRelativePath('Code/Tools/SDLExtension/src/include'),
		]

	# Setup global library search path
	v['LIBPATH'] += [
		android_stl_home + 'libs/armeabi-v7a',
		platform_target + '/usr/lib'
	]
	# Introduce the compiler to generate 32 bit code
	v['CFLAGS'] += [ '-mfpu=neon', '-mfloat-abi=softfp', '-march=armv7-a' ]
	v['CXXFLAGS'] += [ '-mfpu=neon', '-mfloat-abi=softfp', '-march=armv7-a' ]
	
	v['CFLAGS'] += [ '--sysroot=' + android_ndk_platform_compiler_target ]
	v['CXXFLAGS'] += [ '--sysroot=' + android_ndk_platform_compiler_target ]
	v['LINKFLAGS'] += [ '--sysroot=' + android_ndk_platform_compiler_target, android_stl_home + '/libs/armeabi-v7a/libgnustl_shared.so']


@conf
def load_debug_win_x64_android_arm_gcc_settings(conf):
	"""Setup all compiler and linker settings shared over all win_x64_arm_linux_androideabi_4_8 configurations for
	the 'debug' configuration
	"""
	v = conf.env
	conf.load_win_x64_android_arm_gcc_common_settings()

	## Load addional shared settings
	conf.load_debug_cryengine_settings()
	conf.load_debug_gcc_settings()
	#conf.load_debug_linux_settings()
	#conf.load_debug_linux_x64_settings()


@conf
def load_profile_win_x64_android_arm_gcc_settings(conf):
	"""Setup all compiler and linker settings shared over all win_x64_arm_linux_androideabi_4_8 configurations for
	the 'profile' configuration
	"""
	v = conf.env
	conf.load_win_x64_android_arm_gcc_common_settings()

	# Load addional shared settings
	conf.load_profile_cryengine_settings()
	conf.load_profile_gcc_settings()
	#conf.load_profile_linux_settings()
	#conf.load_profile_linux_x64_settings()	


@conf
def load_performance_win_x64_android_arm_gcc_settings(conf):
	"""Setup all compiler and linker settings shared over all win_x64_arm_linux_androideabi_4_8 configurations for
	the 'performance' configuration
	"""
	v = conf.env
	conf.load_win_x64_android_arm_gcc_common_settings()

	# Load addional shared settings
	conf.load_performance_cryengine_settings()
	conf.load_performance_gcc_settings()
	#conf.load_performance_linux_settings()
	#conf.load_performance_linux_x64_settings()


@conf
def load_release_win_x64_android_arm_gcc_settings(conf):
	"""Setup all compiler and linker settings shared over all win_x64_arm_linux_androideabi_4_8 configurations for
	the 'release' configuration
	"""
	v = conf.env
	conf.load_win_x64_android_arm_gcc_common_settings()

	# Load addional shared settings
	conf.load_release_cryengine_settings()
	conf.load_release_gcc_settings()
	#conf.load_release_linux_settings()
	#conf.load_release_linux_x64_settings()


from waflib.TaskGen import feature, before_method
from waflib import TaskGen, Task, Utils
import os
import shutil

@feature('android_launcher')
def process_android_java_files(self):

	if not 'android' in self.bld.cmd or self.bld.cmd == "msvs":
		return 

	bld = self.bld
	platform = bld.env['PLATFORM']
	configuration = bld.env['CONFIGURATION']
	bin_temp_path = bld.bldnode.make_node('BinTemp')
	final_bin_outpath = bld.get_output_folders(platform, configuration)[0]
	apk_folder = bin_temp_path.make_node('apk_data')

	try:
		# TO DO: OPTIMIZE
		# Clear everything as everything in this folder contributes to the apk file. Ensure we are not using leftover stuff.
		shutil.rmtree(apk_folder.abspath())
	except:
		pass
	finally:
		apk_folder.mkdir()

	# Output folders
	javac_output_path = bin_temp_path.make_node('javac_output')
	javac_target_output_path = javac_output_path.make_node(self.target)
	jar_file_path = javac_output_path.make_node(self.target + '.jar')

	try:
		# TO DO: OPTIMIZE
		# Clear everything as everything in this folder contributes to the jar file. Ensure we are not using leftover .class files etc. 
		shutil.rmtree(javac_output_path.abspath())
	except:
		pass
	finally:
		javac_output_path.mkdir()

	# Ensure JAVA_HOME is set to the correct directory otherwise Ant may force JAVA_HOME to the Java Runtime Environment  directory 
	# which does not have a tools packed with it that are required
	os.environ['JAVA_HOME'] = str(self.env['ANDROID_JAVA_HOME'])

	# Java compile
	self.java_source = self.to_nodes(getattr(self, 'java_source', []))
	self.javac_task = self.create_task('javac', self.java_source)
	self.javac_task.output_dir = self.output_dir = javac_target_output_path
	self.javac_task.output_dir.mkdir()  

	# Java archive
	self.jar_task = self.create_task('jar_create', None, jar_file_path)
	self.jar_task.basedir = self.javac_task.output_dir
	self.jar_task.env['JAROPTS'] = [ javac_target_output_path.abspath() + '\\*']
	self.jar_task.env['JARROOT'] = javac_target_output_path.abspath()
	self.jar_task.set_run_after(self.javac_task)
	self.javac_task.dependent_task = [self.jar_task]

	# Copy .jar file to apk_date folder
	lib_path = apk_folder.make_node('libs')
	jar_final_path = lib_path.make_node(self.target + '.jar')
	copy_libs_task = self.create_task('copy_outputs', jar_file_path, jar_final_path )
	lib_path.mkdir()

	# Create resource (AndroidManifest)
	template = compile_template(APP_MANIFEST_STRING_TEMPLATE)
	string_xml_data = {'app_name' : android_app_name }
	string_xml_dir = apk_folder.make_node('res/values')
	string_xml_node = string_xml_dir.make_node('string.xml')
	string_xml_task = self.create_task('generate_file', None, string_xml_node)
	string_xml_task.file_content = template(string_xml_data)
	string_xml_dir.mkdir()

	# Create android manifest
	template = compile_template(APP_MANIFEST_TEMPLATE)
	app_manifest_xml_data = {
		'package': android_package_name,
		'version_code': android_version_code,
		'version_name': android_version_name,
		'debuggable': android_debuggable,
		'min_sdk_version': android_min_sdk_version,
		'premissions_required': android_permissions_required
	}
	app_manifest_xml_node = apk_folder.make_node('AndroidManifest.xml')
	app_manifest_xml_task = self.create_task('generate_file', None, app_manifest_xml_node)
	app_manifest_xml_task.file_content =  template(app_manifest_xml_data)

	# Generate gdb.setup file (GDB)
	gdb_setup_node = final_bin_outpath.make_node('gdb.setup')
	gdb_setup_task = self.create_task('generate_file', None, gdb_setup_node)
	gdb_setup_task.file_content = 'set solib-search-path ' + final_bin_outpath.make_node('lib_debug/armeabi-v7a').abspath().replace('\\', '/')

	# Create build.xml (ANT)
	build_xml_node = apk_folder.make_node('build.xml')
	build_xml_task = self.create_task('generate_file', None, build_xml_node)
	build_xml_task.file_content =  ANT_BUILD_TEMPLATE

	# Create build.properties (ANT)
	build_properties_node = apk_folder.make_node('build.properties')
	build_properties_task = self.create_task('create_ant_properties', None, build_properties_node)

	# Clean ant task
	inputs = [jar_final_path, build_xml_node, string_xml_node, app_manifest_xml_node, build_properties_node]
	self.ant_clean_task = self.create_task('execute_ant', inputs, None)
	self.ant_clean_task.output_dir = apk_folder
	self.ant_clean_task.command = 'clean'

	# Build ant task
	ant_apk_node = apk_folder.make_node('bin').make_node(self.target + '-debug.apk')
	self.ant_build_task = self.create_task('execute_ant', None, ant_apk_node)
	self.ant_build_task.output_dir = apk_folder
	self.ant_build_task.command = 'debug'
	self.ant_build_task.set_run_after(self.ant_clean_task)

	apk_folder.make_node('src').mkdir()
	apk_folder.make_node('gen').mkdir()
	apk_folder.make_node('bin').mkdir()
	apk_folder.make_node('build').mkdir()	

	# Copy .apk file to output folder
	#ant_apk_node = apk_folder.make_node('bin').make_node(self.target + '-debug.apk') #  NOTE THIS IS DEBUG!!! need to handle profile/compile, too
	final_apk_output_path = final_bin_outpath.make_node(self.target +'.apk')
	copy_libs_task = self.create_task('copy_outputs', ant_apk_node, final_apk_output_path )
	copy_libs_task.set_run_after(self.ant_build_task)


###############################################################################
""" Code based on WAF javaw.py file, written by Thomas Nagy """	
class javac(Task.Task):
	"""Compile java files
	"""
	color   = 'BLUE'

	nocache = True
	"""The .class files cannot be put into a cache at the moment """

	vars = ['JAVAC', 'ANDROID_CLASSPATH']
	"""The javac task will be executed again if the variables CLASSPATH, JAVACFLAGS, JAVAC or OUTDIR change. """

	update_outputs = True
	def scan(self):
		return (self.generator.bld.node_deps.get(self.uid(), []), [])

	def run(self):
		""" Execute the javac compiler"""
		env = self.env
		gen = self.generator
		bld = gen.bld

		cmd = []
		cmd += [ env['JAVAC'] ]
		cmd += [ '-classpath', env['ANDROID_CLASSPATH'] ]
		cmd += [ '-d', self.output_dir.abspath() ]
		cmd += [ '-Xlint:deprecation' ]

		#files = [a.path_from(bld.bldnode) for a in self.inputs]
		files = [a.path_from(self.output_dir) for a in self.inputs]

		# workaround for command line length limit:
		# http://support.microsoft.com/kb/830473
		tmp = None
		try:
			if len(str(files)) + len(str(cmd)) > 8192:
				(fd, tmp) = tempfile.mkstemp(dir=bld.bldnode.abspath())
				try:
					os.write(fd, '\n'.join(files).encode())
				finally:
					if tmp:
						os.close(fd)
				if Logs.verbose:
					Logs.debug('runner: %r' % (cmd + files))
				cmd.append('@' + tmp)
			else:
				cmd += files

			ret = self.exec_command(cmd, cwd=self.output_dir.abspath(), env=env.env or None)
		finally:
			if tmp:
				os.remove(tmp)
		return ret

	def post_run(self):
		"""
		"""
		class_nodes = self.generator.output_dir.ant_glob('**/*.class')
		self.generator.bld.node_deps[self.uid()] = class_nodes
		try:
				del self.cache_sig
		except:
				pass

		#class_nodes = self.generator.output_dir.ant_glob('**/*.class')
		#self.generator.bld.node_deps[self.uid()] = class_nodes
		self.outputs = class_nodes

		for dep_task in self.dependent_task:
			dep_task.inputs.extend(class_nodes)

		Task.Task.post_run(self)
		##Task.update_outputs(self)
		##update_outputs=True

class jar_create(Task.Task):
	"""Create a jar file
	"""
	color   = 'GREEN'
	run_str = '${JAR} cf ${TGT} -C ${JARROOT} .' # see http://docs.oracle.com/javase/7/docs/technotes/tools/windows/jar.html

	nocache = True

	def runnable_status(self):
		"""Wait for dependent tasks to be executed, then read the
		files to update the list of inputs.
		"""

		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER

		if not self.inputs:
			try:
				self.inputs = [x for x in self.basedir.ant_glob('**/*', remove=False) if id(x) != id(self.outputs[0])]
			except Exception:
				raise Errors.WafError('Could not find the basedir %r for %r' % (self.basedir, self))

		if not os.path.isfile(self.outputs[0].abspath()):
			return Task.RUN_ME

		return super(jar_create, self).runnable_status()

###############################################################################
""" Code based on WAF javaw.py file, written by Thomas Nagy """	
class strip_shared_lib(Task.Task):
	"""Compile java files
	"""
	color = 'GREEN'

	vars = ['STRIP']
	#nocache = True

	def runnable_status(self):
		"""Wait for dependent tasks to be executed, then read the
		files to update the list of inputs.
		"""

		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER

		if not os.path.isfile(self.outputs[0].abspath()):
			return Task.RUN_ME

		return super(strip_shared_lib, self).runnable_status()

	def run(self):
		""" Execute the javac compiler """
		env = self.env
		gen = self.generator
		bld = gen.bld

		src = self.inputs[0].abspath()
		tgt = self.outputs[0].abspath()

		cmd = []
		cmd += [ env['STRIP'] ]
		cmd += [ src ]
		cmd += [ '--strip-debug' ]
		cmd += [ '-o', tgt ]

		ret = self.exec_command(cmd, env=env.env or None)
		return ret

###############################################################################
# Function to generate the copy tasks for build outputs	
@after_method('add_install_copy')
@after_method('process_android_java_files')
@feature('c', 'cxx')
def add_android_lib_copy(self):
	if not 'android' in self.bld.cmd or self.bld.cmd == "msvs":
		return 

	if not getattr(self, 'link_task', None):
		return

	if self._type == 'stlib': # Do not copy static libs
		return

	bld = self.bld
	platform	= bld.env['PLATFORM']
	configuration	= bld.env['CONFIGURATION']
	bin_temp_path = bld.bldnode.make_node('BinTemp')
	apk_folder = bin_temp_path.make_node('apk_data')

	apk_lib_folder = apk_folder.make_node('lib')

	# Add external libs
	android_shared_libs = { 'armeabi-v7a': [
		bld.env['ANDROID_SDL_HOME'] + '/libs/armeabi-v7a/libgnustl_shared.so',
		bld.env['ANDROID_NDK_HOME'] + '/prebuilt/android-arm/gdbserver/gdbserver',
		bld.CreateRootRelativePath('Code/Tools/SDLExtension/lib/android-armeabi-v7a/libSDL2Ext.so'),
		bld.CreateRootRelativePath('Code/SDKs/SDL2/lib/android-armeabi-v7a/libSDL2.so')
	]}

	# Add internal libs
	if not 'armeabi-v7a' in android_shared_libs:
		android_shared_libs['armeabi-v7a'] = []
	for src in self.link_task.outputs:
		android_shared_libs['armeabi-v7a'].append(src.abspath())

	# Original shared lib output path (not stripped)(for debugger)
	debug_lib_output = bld.get_output_folders(platform, configuration)[0].make_node('lib_debug')
	debug_lib_output.mkdir()

	for lib_dir, shared_libs in android_shared_libs.iteritems():
		tgt_dir = apk_lib_folder.make_node(lib_dir)
		tgt_dir.mkdir()

		tgt_debug = debug_lib_output.make_node(lib_dir)
		tgt_debug.mkdir()

		for shared_lib in shared_libs:
			shared_lib_basename = os.path.basename(shared_lib)
			src = bld.root.make_node(shared_lib)

			# Copy original to debug lib dir (for debugger)
			self.create_task('copy_outputs', src, tgt_debug.make_node(shared_lib_basename))

			# Strip lib and add to apk directory
			tgt_stripped = tgt_dir.make_node(shared_lib_basename)
			task = self.create_task('strip_shared_lib',src, tgt_stripped)
			self.ant_clean_task.set_run_after(task)

###############################################################################
class generate_file(Task.Task):
	"""Compile java files
	"""
	color = 'YELLOW'

	#vars = ['STRIP']
	nocache = True
	file_content = ""

	def runnable_status(self):
		"""Wait for dependent tasks to be executed, then read the
		files to update the list of inputs.
		"""

		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER

		if not os.path.isfile(self.outputs[0].abspath()):
			return Task.RUN_ME

		return super(generate_file, self).runnable_status()

	def run(self):
		""" Execute the javac compiler """
		self.outputs[0].stealth_write(rm_blank_lines(self.file_content))
		return 0

###############################################################################
class create_ant_properties(Task.Task):
	"""Compile java files
	"""
	color = 'YELLOW'

	#vars = ['STRIP']
	nocache = True
	properties_list = []

	def runnable_status(self):
		"""Wait for dependent tasks to be executed, then read the
		files to update the list of inputs.
		"""

		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER

		if not os.path.isfile(self.outputs[0].abspath()):
			return Task.RUN_ME

		return super(create_ant_properties, self).runnable_status()

	def run(self):
		""" Execute the javac compiler """
		env = self.env
		output_str = []
		output_str.append('# This file is automatically generated by Android Tools.')
		output_str.append('# Do not modify this file -- YOUR CHANGES WILL BE ERASED!')
		output_str.append('')
		output_str.append('target=android-%s' % str(env['ANDROID_TARGET_VERSION']))
		output_str.append('')
		output_str.append('gen.absolute.dir=./gen')
		output_str.append('out.dir=./bin')
		output_str.append('jar.libs.dir=./libs')
		output_str.append('native.libs.absolute.dir=./lib')
		output_str.append('sdk.dir=%s' % str(env['ANDROID_SDK_HOME']).replace('\\','/'))
		self.outputs[0].stealth_write('\n'.join(output_str))
		return 0

###############################################################################
class execute_ant(Task.Task):
	command = ''
	output_dir = None

	def runnable_status(self):
		"""Wait for dependent tasks to be executed, then read the
		files to update the list of inputs.
		"""

		for t in self.run_after:
			if not t.hasrun:
				return Task.ASK_LATER

		return Task.RUN_ME

	def run(self):
		""" Run Ant """
		env = self.env
		gen = self.generator

		cmd = []
		cmd += [ env['ANT'] ]
		cmd += [ self.command ]
		
		ret = self.exec_command(cmd, cwd=self.output_dir.abspath(), env=env.env or None)
		return ret

###############################################################################
# Function to generate the copy tasks for build outputs	
@after_method('process_source')
@before_method('apply_link')
@feature('android_hack')
def force_add_dependent_android_files(self):

	# ANDROID HACK
	# Add android dependent files from none src or build folder to build pipeline

	if not 'android' in self.bld.cmd or self.bld.cmd == "msvs":
		return 

	bld = self.bld
	env = bld.env

	special_sources = []
	android_sdl_home = self.bld.env['ANDROID_SDL_HOME']
	if android_sdl_home:
		special_sources += [ bld.root.make_node(bld.CreateRootRelativePath('/Code/Tools/SDLExtension/src/jni/SDLExt_android.c')) ]
		special_sources += [ bld.root.make_node(bld.CreateRootRelativePath('/Code/Tools/SDLExtension/src/jni/SDLExt_android_init.c')) ]

	android_ndk_home = bld.env['ANDROID_NDK_HOME']
	if android_ndk_home:
		special_sources += [ bld.root.make_node(android_ndk_home + '/sources/android/native_app_glue/android_native_app_glue.c') ]

	# Create compile tasks
	self.source +=  special_sources
	for node in special_sources: 
		self.get_hook(node)(self, node)

