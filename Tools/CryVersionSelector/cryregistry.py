#!/usr/bin/env python3

import os
import json

from win32com.shell import shell, shellcon

ENGINE_FILENAME = 'cryengine.json'
ENGINE_EXTENSION = '.cryengine'

def paths_enginedb():
	#https://blogs.msdn.microsoft.com/patricka/2010/03/18/where-should-i-store-my-data-and-configuration-files-if-i-target-multiple-os-versions/
	return [os.path.join (os.path.expandvars('%userprofile%'), '.cryengine', ENGINE_FILENAME)]

# For backwards compatibility the old paths are still looked up to load data, but saving is only done to paths_enginedb.
def paths_enginedb_old():
	return (
		os.path.join (shell.SHGetFolderPath (0, shellcon.CSIDL_COMMON_APPDATA, None, 0), 'Crytek', 'CRYENGINE', ENGINE_FILENAME),
		os.path.join (shell.SHGetFolderPath (0, shellcon.CSIDL_LOCAL_APPDATA, None, 0), 'Crytek', 'CRYENGINE', ENGINE_FILENAME)
	)

def delete():
	for registry_path in paths_enginedb_old():
		try:
			if os.path.isfile (registry_path):
				os.remove (registry_path)
		except:
			pass

	for registry_path in paths_enginedb():
		try:
			if os.path.isfile (registry_path):
				os.remove (registry_path)
		except:
			pass

def load_engines():
	db = {}

	#Load old engine data first.
	for registry_path in paths_enginedb_old():
		try:
			with open (registry_path, 'r') as file:
				db.update (json.loads (file.read()))
		except:
			pass

	for registry_path in paths_enginedb():
		try:
			with open (registry_path, 'r') as file:
				db.update (json.loads (file.read()))
		except:
			pass

	return db

def save_engines (db):
	for registry_path in paths_enginedb():
		try:
			registry_dir = os.path.dirname (registry_path)
			if not os.path.isdir (registry_dir):
				os.makedirs (registry_dir)

			with open (registry_path, 'w') as file:
				file.write (json.dumps (db, indent = 4, sort_keys = True))
		except:
			print("Error while saving  to " + registry_path)
			pass

#---

def engine_path (db, engine_version):
	path = db.get (engine_version, {'uri': None})['uri']
	return path and os.path.dirname (path) or None
