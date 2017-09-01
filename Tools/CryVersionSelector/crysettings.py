#!/usr/bin/env python3

import json
import os.path
import sys

class Settings:
    def __init__(self):
        self.settings = {}
        self.load()

    def get_filepath(self):
        if getattr( sys, 'frozen', False ):
            scriptpath = sys.executable
        else:
            scriptpath = __file__
        path = os.path.dirname(os.path.realpath(scriptpath))
        return os.path.join(path, "settings.cfg")

    def load(self):
        filepath = self.get_filepath()
        if not os.path.isfile(filepath):
            return
        try:
            with open (filepath) as fd:
                self.settings = json.loads (fd.read())
        except ValueError:
            self.settings = {}

    def save(self):
        filepath = self.get_filepath()
        with open(filepath, 'w') as fd:
            json.dump(self.settings, fd, indent=4, sort_keys=True)

    def get_last_cmake_config(self):
        return self.settings.get('last_cmake_config', None)

    def set_last_cmake_config(self, id):
        self.settings['last_cmake_config'] = id
        self.save()