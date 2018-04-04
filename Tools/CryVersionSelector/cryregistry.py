#!/usr/bin/env python3
'''
Handles loading and saving the available engines on this machine.
'''

import os
import json
import platform

ENGINE_FILENAME = 'cryengine.json'
ENGINE_EXTENSION = '.cryengine'

def paths_engine_database():
    '''
    Returns the path to the files where the engines are registered.
    '''
    #https://blogs.msdn.microsoft.com/patricka/2010/03/18/where-should-i-store-my-data-and-configuration-files-if-i-target-multiple-os-versions/
    if platform.system() == "Windows":
        return [os.path.join(os.path.expandvars("%ALLUSERSPROFILE%"), "Crytek", "CRYENGINE", ENGINE_FILENAME)]
    return [os.path.join(os.path.expandvars("$HOME"), ".cryengine", ENGINE_FILENAME)]

# For backwards compatibility the old paths are still looked up to load data, but saving is only done to paths_enginedb.
def paths_engine_database_old():
    '''
    Returns the path to the files where the engines used to be registered.
    '''

    if platform.system() == "Windows":
        return [
            os.path.join(os.path.expandvars('%USERPROFILE%'), '.cryengine', ENGINE_FILENAME),
            os.path.join(os.path.expandvars("%LOCALAPPDATA%"), "Crytek", "CRYENGINE", ENGINE_FILENAME)
        ]
    return []

def delete():
    '''
    Deletes all databases with registered engines.
    '''
    for registry_path in paths_engine_database_old():
        try:
            if os.path.isfile(registry_path):
                os.remove(registry_path)
        except:
            pass

    for registry_path in paths_engine_database():
        try:
            if os.path.isfile(registry_path):
                os.remove(registry_path)
        except:
            pass

def load_engines():
    '''
    Returns a database of all registered engines.
    '''
    database = {}

    #Load old engine data first.
    for registry_path in paths_engine_database_old():
        try:
            if os.path.isfile(registry_path):
                with open(registry_path, 'r') as file:
                    database.update(json.loads(file.read()))
        except:
            pass

    for registry_path in paths_engine_database():
        try:
            if os.path.isfile(registry_path):
                with open(registry_path, 'r') as file:
                    database.update(json.loads(file.read()))
        except:
            pass

    return database

def save_engines(database):
    '''
    Saves the database to the hard-drive.
    '''
    for registry_path in paths_engine_database():
        try:
            registry_dir = os.path.dirname(registry_path)
            if not os.path.isdir(registry_dir):
                os.makedirs(registry_dir)

            with open(registry_path, 'w') as file:
                file.write(json.dumps(database, indent=4, sort_keys=True))
        except:
            print("Error while saving  to " + registry_path)

def engine_path(database, engine_version):
    '''
    Returns the path to the root-folder of the specified engine version,
    or None if the engine version is not in the database.
    '''
    path = database.get(engine_version, {'uri': None})['uri']

    if not path:
        return None

    root = os.path.dirname(path)
    return root if root else None
