#!/usr/bin/env python3

import json
import os.path

def load(path):
    try:
        file = open(path, 'r', encoding='utf-8')
        proj = json.loads (file.read())
        file.close()
    except ValueError:
        proj = None

    return proj

def save (self, path):
    file= open (path, 'w')
    file.write (json.dumps (self, indent=4, sort_keys=True))
    file.close()
    
def is_valid (self):
    return True

def engine_id(self):
    return self.get ('require', {}).get ('engine')

    
def shared_dir(self, platform, config):
    lib = libs_list(self)[0]
    if lib is None:
        return os.path.join ("bin", platform)

    return os.path.dirname (lib.get('shared', {}).get (platform))

def asset_dir(self):
    return self['content'].get ('assets', [None])[0]

def cmakelists_dir(self):
    return self['content'].get ('code', [None])[0]

def require_list(self):
    return self.get ('require', [])

def plugins_list(self):
    return self.get ('require', {}).get ('plugins')

def libs_list(self):
    return self['content'].get ('libs', [None])

def is_managed(self):
    plugins = plugins_list(self)
    for plugin in plugins:
        if plugin.get('type') == "EPluginType::Managed":
            return True

    return False