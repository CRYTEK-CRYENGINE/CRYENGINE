# Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
from waflib.Errors import BuildError, WafError
from waflib.Configure import conf
from waflib import Errors, Logs, Utils

import cPickle as pickle
import thread
import threading
import sys
import Queue
import subprocess

import os
import socket
import atexit
import signal

try:
	import _winreg
except:
	pass
	
class Local_CommandCoordinator(object):
		
	def execute_command(self, cmd, **kw):
		try:
			if kw['stdout'] or kw['stderr']:
				p = subprocess.Popen(cmd, **kw)
				(out, err) = p.communicate() # [blocking]
				ret = p.returncode
			else:
				out, err = (None, None)
				ret = subprocess.Popen(cmd, **kw).wait()
		except Exception as e:
			raise Errors.WafError('Execution failure: %s (%s)' % (str(e), cmd), ex=e)

		# strip IB xgTaskTags (temp, Xoreax is aware and is fixing the issue)
		task_id = '{{xgTaskID=00000000}}\r\n'
		task_id_len = len(task_id)		
		
		if out.endswith(task_id):
			out = out[:-task_id_len]

		if err.endswith(task_id):
			err = err[:-task_id_len]
	
		return (ret, out, err)
		
###########################################
################## IB #######################
###########################################
socket_port = 3288
command_end_keyword = "!end"

@Utils.memoize
def get_ib_folder():
	IB_settings = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\Xoreax\\Incredibuild\\Builder", 0, _winreg.KEY_READ)
	(ib_folder,type) = _winreg.QueryValueEx(IB_settings, 'Folder')
	return str(ib_folder)
	
@Utils.memoize
def get_ib_licence_string():
	result = subprocess.check_output( [ str(get_ib_folder())+ '/xgconsole.exe', '/QUERYLICENSE'])
	return result

def execute_waf_via_ib(bld):	
	# Check if we can execute remotely with our current licenses	
	if not bld.is_option_true('use_incredibuild_win') and ('win32' in bld.cmd or 'win64' in bld.cmd):
		Logs.warn('[WARNING] Incredibuild for Windows targets disabled by build option')
		return False
	
	if not bld.is_option_true('use_incredibuild_win') and ('win_x86' in bld.cmd or 'win_x64' in bld.cmd):
		Logs.warn('[WARNING] Incredibuild for Windows targets disabled by build option')
		return False
		
	if not bld.is_option_true('use_incredibuild_durango') and 'durango' in bld.cmd:
		Logs.warn('[WARNING] Incredibuild for Durango targets disabled by build option')
		return False
		
	if not bld.is_option_true('use_incredibuild_orbis') and 'orbis' in bld.cmd:
		Logs.warn('[WARNING] Incredibuild for Orbis targets disabled by build option')
		return False
	
	result = get_ib_licence_string()
	
	has_make_and_build_license = 'Make && Build Tools' in result
	has_dev_tools_acceleration_license = 'Dev Tools Acceleration' in result	
	has_multi_core_license = 'Cores' in result
	
	if not has_make_and_build_license and not has_dev_tools_acceleration_license:
		Logs.warn('Neither "Make && Build Tools" nor " Dev Tools Acceleration"	package found. You need a least one. Incredibuild disabled.')
		return False
		
	if not 'PlayStation' in result and 'orbis' in bld.cmd:
		Logs.warn('Playstation Extension Package not found! Incredibuild will build locally only.')

	#Disabled as current build pipeline does not require the licence
	#if not 'Xbox One' in result and 'durango' in bld.cmd:
	#	Logs.warn('Xbox One Extension Package not found! Incredibuild will build locally only.')

	cmd_line_args = []
	for arg in sys.argv[1:]:
		if arg == 'generate_uber_files':
			continue
		cmd_line_args += [ arg ]

	command_line_options = ' '.join(cmd_line_args) # Recreate command line
    
	# Add special option to not start IB from within IB
	command_line_options += ' --internal-dont-check-recursive-execution=True'
	
	# Build Command Line
	cry_waf = bld.path.make_node('Code/Tools/waf-1.7.13/bin/cry_waf.exe')
	command = cry_waf.abspath() + ' ' + command_line_options
	
	if bld.is_option_true('run_ib_as_service'):
		Logs.info('[WAF] Starting Incredibuild as "Build Service"')
		allow_local = "/AvoidLocal=On"
	else:
		Logs.info('[WAF] Starting Incredibuild as "Build Master"')
		allow_local = "/AvoidLocal=Off"
		
	if not has_multi_core_license:
		allow_local = "/AvoidLocal=On"
		
	# Get correct incredibuild installation folder to not depend on PATH
	ib_folder = get_ib_folder()
	if (has_dev_tools_acceleration_license and not has_make_and_build_license)  or 'linux' in bld.variant or 'android' in bld.variant or 'cppcheck' in bld.variant:
		try:			
			p = subprocess.Popen ([str(ib_folder) + '/xgconsole.exe', "/command=" + command, "/profile=Code\\Tools\\waf-1.7.13\\profile.xml", "/useidemonitor", "/nologo", allow_local])
		except:
			raise BuildError()
	else:
		try:
			p = subprocess.Popen ([ str(ib_folder) + '/BuildConsole.exe', "/command=" + command, "/useidemonitor", "/nologo", allow_local])
		except:
			raise BuildError()
			
	if not bld.instance_is_build_master():
		sys.exit(p.wait())
		
	return True

def handle_connection_thread(client_connection):
	socket_data_str = ''
	
	while True:
		# Receive data 
		data = client_connection.recv(4096) # [blocking]
		socket_data_str += data
		# Check for end command token
		if socket_data_str.endswith(command_end_keyword):
			socket_data_str = socket_data_str[:-len(command_end_keyword)]
			break

	# Load and execute command
	(cmd, kw) = pickle.loads(socket_data_str)
			
	command_coordinator = Local_CommandCoordinator()
	(ret, out, err) = command_coordinator.execute_command(cmd, **kw)
	
	# Sent executed command reponse
	pickle_str = pickle.dumps((ret, out, err))
	client_connection.sendall(pickle_str)
		
	# Close connection
	client_connection.close()
		
#### SERVER####
class IB_CommandCoordinator_Server(object):		
	def enter_command_loop(self, bld):
		serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		serversocket.bind(('localhost', socket_port))
		serversocket.listen(1024) 

		while True:
			# Wait for connection
			client_connection, client_address = serversocket.accept() # [blocking]
			thread.start_new_thread(handle_connection_thread, (client_connection,))

class IB_CommandCoordinator_Client(Local_CommandCoordinator):

	def __init__(self, bld):	
		
		self.ib_remote_executables = {
		os.path.basename(bld.env['CXX']): True,
		os.path.basename(bld.env['CC'])  : True
		}	

	def execute_command(self, cmd, **kw):	
		# Commands that can be executed remotly
		if self.ib_remote_executables.get(os.path.basename(cmd[0]), False): 
			(ret, out, err) = self.submit_remote_command(cmd, **kw)
			
			#if ret == 0:
			#	if basename in ['rcc.exe']:			
			#		self.submit_remote_command(['F:\P4\CE_STREAMS\BinTemp\touch.bat', cmd[-1]] , **kw)
			#	elif basename in ['moc.exe']:
			#		self.submit_remote_command(['F:\P4\CE_STREAMS\BinTemp\touch.bat', cmd[-2][2:]] , **kw)			
		else:
			(ret, out, err) = super(IB_CommandCoordinator_Client, self).execute_command(cmd, **kw)

		return (ret, out, err)		
		
	def submit_remote_command(self, cmd, **kw):
		# Try to connect
		clientsocket = None
		while True:
			try:
				clientsocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
				clientsocket.connect(('localhost', socket_port))
				break
			except:
				continue
				
		# Send command to server
		cmd_tuple = (cmd, kw)
		pickle_string = pickle.dumps(cmd_tuple)
		clientsocket.sendall(pickle_string + command_end_keyword)
		
		# Wait for server response
		answer_str = ''
		while True:
			data = clientsocket.recv(4096) # [blocking]
			if not data:  # check for closed connection
				break			
			answer_str += data
		clientsocket.close()
		
		# return response
		return pickle.loads(answer_str)	
		
############
### IB END ###
############

@conf
def set_cmd_coordinator(conf, coordinator_name):

	if coordinator_name == 'Local':
		conf.is_build_master = True
		conf.cmd_coordinator = Local_CommandCoordinator()		
	elif coordinator_name == 'IB':		
		jobs_backup = conf.jobs
		conf.options.jobs =  int(conf.options.incredibuild_max_cores) + conf.options.jobs 
		conf.jobs = conf.options.jobs	
		
		# If a multi core licence is available, run IB as build master		
		run_ib_as_service = conf.is_option_true('run_ib_as_service')
		is_recursive_ib_instance = conf.is_option_true('internal_dont_check_recursive_execution')		
		
		if not is_recursive_ib_instance:
			if run_ib_as_service and "Cores" in get_ib_licence_string():
				Logs.warn('Incredibuild multicore licence detected. Consider disabling "run_ib_as_service" for faster build times.')

			conf.is_build_master = run_ib_as_service

			if not execute_waf_via_ib(conf):
				conf.is_build_master = True
				conf.options.jobs = jobs_backup
				conf.jobs = jobs_backup
				return

			if run_ib_as_service:
				conf.cmd_coordinator = IB_CommandCoordinator_Client(conf)
				
		elif run_ib_as_service:
			Logs.info("[WAF] Run Incredibuild as a service")
			conf.is_build_master = False
			conf.cmd_coordinator = IB_CommandCoordinator_Server()
			conf.cmd_coordinator.enter_command_loop(conf)
		else:
			conf.is_build_master = True
			conf.cmd_coordinator = Local_CommandCoordinator()

@conf
def instance_is_build_master(conf):
	return conf.is_build_master
	
@conf
def setup_command_coordinator(bld):
	coordinator = 'Local'
	if not Utils.unversioned_sys_platform() == 'win32':
		bld.set_cmd_coordinator('Local') # Don't use recursive execution on non-windows hosts
		return
	
	if 'clean_' in bld.cmd or 'generate_uber_files' in bld.cmd or 'msvs' in bld.cmd:
		bld.set_cmd_coordinator('Local')
		return

	# Don't use IB for special single file operations
	if bld.is_option_true('show_includes') or bld.is_option_true('show_preprocessed_file') or bld.is_option_true('show_disassembly') or bld.options.file_filter != "":
		bld.set_cmd_coordinator('Local')
		return
		
	if  bld.is_option_true('use_incredibuild'):
		bld.set_cmd_coordinator('IB')
		return
	else:
		Logs.warn('[WARNING] Incredibuild disabled by build option')
		bld.set_cmd_coordinator('Local')
		return
