import os.path
import argparse
from argparse import ArgumentParser
from shutil import copy
import sys
import glob
import subprocess

def get_vcvarsall_path():
	import _winreg as winreg

	possible_roots = ['SOFTWARE\\Wow6432node', 'SOFTWARE']
	vc_dir = None
	for root in possible_roots:
		try:
			key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, root + "\\Microsoft\\VisualStudio\\14.0\\Setup\\VC")
			vc_dir = winreg.QueryValueEx(key, 'ProductDir')[0]
			key.Close()
			break
		except:
			continue

	if vc_dir is None:
		raise RuntimeError('Could not detect a compatible installation of Visual Studio')

	vcvarsall_path = os.path.join(vc_dir, 'vcvarsall.bat')
	if not os.path.exists(vcvarsall_path):
		raise RuntimeError('Could not find vcvarsall.bat')

	return vcvarsall_path

def copy_output(src_file, dst_directory):
	print('Copying ' + src_file + ' to ' + dst_directory)
	copy(src_file, dst_directory)

def copy_outputs(src_queries, dst_directory):
	if dst_directory is not None:
		for src_query in src_queries:
			results = glob.glob(os.path.normpath(src_query))
			for result in results:
				copy_output(result, dst_directory)

def build(configuration, platform, lib_dir = None, exe_dir = None, portable = False):
	if lib_dir is not None:
		lib_dir = os.path.normpath(os.path.abspath(lib_dir))
		if not os.path.exists(lib_dir):
			os.makedirs(lib_dir)

	if exe_dir is not None:
		exe_dir = os.path.normpath(os.path.abspath(exe_dir))
		if not os.path.exists(exe_dir):
			os.makedirs(exe_dir)

	id_platform_flags = platform
	if portable:
		id_platform_flags += '_portable'
	script_dir = os.path.dirname(os.path.realpath(__file__))
	build_dir = os.path.join(script_dir, os.pardir, id_platform_flags + '_' + configuration, 'build')

	if not os.path.exists(build_dir):
		os.makedirs(build_dir)
	os.chdir(build_dir)

	configuration_name = {'release': 'Release', 'debug': 'Debug'}[configuration]

	exe_queries = []
	lib_queries = []

	if platform in ['win32', 'win64']:
		platform_name = {'win32' : 'Win32', 'win64' : 'Win64'}[platform]
		
		if platform == 'win64':
			platform_name = 'x64'
			generator_suffix = ' Win64'
			vcvarsall_arg = ' x86_amd64'
		else:
			platform_name = 'Win32'
			generator_suffix = ''
			vcvarsall_arg = ''

		flags = ''
		if portable:
			flags += ' -DPORTABLE=ON'

		commands = ['cmake -G "Visual Studio 14' + generator_suffix + '"'+ flags + ' "' + script_dir + '"']

		vcvarsall_path = get_vcvarsall_path()
		
		commands += ['"' + vcvarsall_path + '"' + vcvarsall_arg]
		commands += ['msbuild.exe HLSLCrossCompilerProj.sln /p:Configuration=' + configuration_name + ' /p:Platform=' + platform_name]
		cmd_line = '&&'.join(commands)
		p = subprocess.Popen(cmd_line, shell = True)
		p.wait()

		exe_queries += [os.path.join(build_dir, os.pardir, 'bin', id_platform_flags, configuration_name, '*.*')]
		lib_queries += [os.path.join(build_dir, os.pardir, 'lib', id_platform_flags, configuration_name, '*.*')]
	elif platform == 'linux':		
		subprocess.call(['cmake', script_dir,'-DCMAKE_BUILD_TYPE:STRING=' + configuration_name])
		subprocess.call(['make', 'libHLSLcc'])
		subprocess.call(['make', 'HLSLcc'])

		exe_queries += [os.path.join(build_dir, os.pardir, 'bin', id_platform_flags, '*')]
		lib_queries += [os.path.join(build_dir, os.pardir, 'lib', id_platform_flags, '*')]
	elif platform == 'android-armeabi-v7a':
		jni_dir = os.path.join(script_dir, os.pardir, 'jni')
		os.chdir(jni_dir)
		subprocess.call(['ndk-build', '-j4'])

		lib_queries += [os.path.join(os.pardir, 'obj', 'local', 'armeabi-v7a', '*.*')]

	copy_outputs(exe_queries, exe_dir)
	copy_outputs(lib_queries, lib_dir)

def main():
	parser = ArgumentParser(description = 'Build HLSLCrossCompiler')
	parser.add_argument(
		'--platform',
		 choices = ['win32', 'win64', 'linux', 'android-armeabi-v7a'],
		 dest = 'platform',
		 required = True)
	parser.add_argument(
		'--configuration',
		choices = ['release', 'debug'],
		dest = 'configuration',
		required = True)
	parser.add_argument(
		'--portable',
		dest = 'portable',
		action = 'store_true')
	parser.add_argument(
		'--lib-output-dir',
		dest = 'lib')
	parser.add_argument(
		'--exe-output-dir',
		dest = 'exe')

	params = parser.parse_args()

	build(params.lib, params.exe, params.configuration, params.platform, params.portable)

if __name__ == '__main__':
	main()