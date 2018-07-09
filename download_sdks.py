#!/usr/bin/env python3
from zipfile import ZipFile
from tarfile import TarFile
from urllib import request

import os
import platform


def print_progress(current, maxprogress):
    status = "%10d  [%3.2f%%]" % (current, current * 100. / maxprogress)
    print(status, end='\r')


def read_chunks(url, block_sz=8192 * 8):
    with request.urlopen(url) as openurl:
        while True:
            data = openurl.read(block_sz)
            if data:
                yield data
            else:
                return


BASE_URL = 'https://github.com/CRYTEK/CRYENGINE/releases/download/5.5.0_preview4/CRYENGINE_v5.5.0_SDKs'


def main():
    # use ZIP on windows and tar on other platforms
    if platform.system() == 'Windows':
        url = BASE_URL + '.zip'
        ArchiveFile = ZipFile
        list_archive = ZipFile.namelist
    else:
        url = BASE_URL + '.tar.gz'
        ArchiveFile = TarFile.open
        list_archive = TarFile.getnames

    temp_folder = '.'
    file_name = url.split('/')[-1]
    temp_file = os.path.join(temp_folder, file_name)

    u = request.urlopen(url)
    meta = u.info()
    file_size = int(u.getheader("Content-Length"))

    print("Downloading: %s Bytes: %s" % (file_name, file_size))

    with open(temp_file, 'wb') as tfile:
        downloaded_bytes = 0
        for chunk in read_chunks(url):
            downloaded_bytes += len(chunk)
            tfile.write(chunk)

            print_progress(downloaded_bytes, file_size)

    print()

    with ArchiveFile(temp_file) as zf:
        nameList = list_archive(zf)
        num_files = len(nameList)
        output_path = 'Code/SDKs'

        print('Extracting %d files to:"%s"' % (num_files, output_path))

        for counter, item in enumerate(nameList, start=1):
            zf.extract(item, output_path)
            print_progress(counter, num_files)

    print()
    os.remove(temp_file)

if __name__== '__main__':
    main()

