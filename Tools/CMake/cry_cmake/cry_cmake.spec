# -*- mode: python -*-

block_cipher = None


a = Analysis(['wrapper.py'],
             pathex=['cry_cmake'],
             binaries=[],
             datas=[('icon.ico', '.')],
             hiddenimports=[],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher)
pyz = PYZ(a.pure, a.zipped_data,
             cipher=block_cipher)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          name='cry_cmake',
          debug=False,
          strip=False,
          upx=False,
          console=False , icon='icon.ico')
