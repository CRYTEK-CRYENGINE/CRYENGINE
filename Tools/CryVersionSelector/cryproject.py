#!/usr/bin/env python3

import cryplugin
import json
import re

class CryProject:
    def __init__(self):
        self.data = {}
        self.path = ''

    def load(self, path):
        file = open(path, 'r')
        fileData = file.read()
        file.close()
        
        ## Make sure to remove xml-style comments before parsing json.
        self.data = json.loads(re.sub('<--[\\s\\S]*?-->', '', fileData))
        self.path = path

    def save(self, path):
        file = open(path, 'w')
        file.write(json.dumps(self.data, indent=4, sort_keys=True))
        file.close()

    def asset_dir(self):
        return self.data.get('content', {}).get('assets', [None])[0]

    def cmakelists_dir(self):
        return self.data.get('content', {}).get('code', [None])[0]

    def engine_id(self):
        return self.data.get('require', {}).get('engine')

    def is_managed(self):
        for plugin in self.plugins_list():
            if 'managed' in plugin.get('type', '').lower():
                return True

            if plugin.get('guid') is not None:
                plugin_file = cryplugin.find(
                    self.data.get(
                        'require', []).get('engine', '.'), plugin['guid'])
                _plugin = cryplugin.CryPlugin()
                try:
                    _plugin.load(plugin_file)
                except Exception:
                    print("Unable to read plugin file %s" % (plugin_file))
                    raise

                return not _plugin.isNative()

        return False

    def name(self):
        return self.data.get('info', {}).get('name')

    def libs_list(self):
        return self.data.get('content', {}).get('libs')

    def plugins_list(self):
        return self.data.get('require', {}).get('plugins')

    def require_list(self):
        return self.data.get('require', [])

    def set_engine_id(self, engine_id):
        self.data['require']['engine'] = engine_id

    def set_plugin_list(self, plugins):
        self.data['require']['plugins'] = plugins
