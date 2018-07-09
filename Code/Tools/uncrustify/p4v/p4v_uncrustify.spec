# To be used with PyInstaller
# Command line: pyinstaller <thisfile> --distfile=.
a = Analysis(['wrapper.py'],
             hiddenimports=['P4', 'platform'],
             runtime_hooks=None)

# Remove specific binaries
a.binaries = a.binaries - TOC([('_hashlib', '', '')])

for bin in a.binaries:
	print('{}'.format(bin))

# Remove clashing dependency duplicate
for d in a.datas:
	if 'pyconfig' in d[0]:
		a.datas.remove(d)
		break

pyz = PYZ(a.pure)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          name=specnm,
          debug=False,
          strip=None,
          upx=True,
          console=True,
)
