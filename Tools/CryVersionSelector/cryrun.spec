# -*- mode: python -*-

block_cipher = None


a = Analysis(['cryrun_exe.py'],
             pathex=['CryVersionSelector'],
             binaries=[],
             datas=[('editor_icon16.ico', '.')],
             hiddenimports=['winshell'],
             hookspath=[],
             runtime_hooks=[],
             excludes=[],
             win_no_prefer_redirects=False,
             win_private_assemblies=False,
             cipher=block_cipher)
pyz = PYZ(a.pure)
exe = EXE(pyz,
          a.scripts,
          name='cryrun',
          debug=False,
          strip=False,
          upx=True,
          exclude_binaries=True,
          console=True , icon='editor_icon16.ico')
coll = COLLECT(exe,
               a.binaries,
               a.zipfiles,
               a.datas,
               strip=None,
               upx=True,
               name='cryrun')