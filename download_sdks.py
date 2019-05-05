#!/usr/bin/env python3
from zipfile import ZipFile
from tarfile import TarFile
from urllib import request

import os
import platform
import json


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


def get_possible_urls(base_repo_id):
    api_url = 'https://api.github.com/repos/' + base_repo_id + '/releases/latest'
    request_headers = {
        "Accept": "application/vnd.github.v3+json"
    }
    req = request.Request(api_url, headers=request_headers)
    response = request.urlopen(req)
    json_result = json.loads(response.read())
    return_list = []
    for release_asset in json_result['assets']:
        if 'browser_download_url' in release_asset:
            return_list.append(release_asset['browser_download_url'])
    return return_list

def get_url(file_extension, links):
    for link in links:
        if link.endswith(file_extension):
            return link
    return None


REPO_ID = 'CRYTEK/CRYENGINE'


def main():
    # get all the assets url in the last release
    links = get_possible_urls(REPO_ID)
    # use ZIP on windows and tar on other platforms
    if platform.system() == 'Windows':
        # get the windows link
        url = get_url('.zip', links)
        if url is None:
            print("An error occurred obtaining the download url for Windows!")
            quit(1)
        ArchiveFile = ZipFile
        list_archive = ZipFile.namelist
    else:
        # get the non-windows link and an alternative link 
        # the alternative link will be asked to use in case the non-windows link can't be found
        url = get_url('.tar.gz', links)
        alternative_url = get_url('.zip', links)
        if url is None:
            print("An error occurred obtaining the download url for your system!")
            if alternative_url is not None:
                answer = input("Do you want to try download the standard .zip file? (y/n)")
                if answer.lower() == "y":
                    url = alternative_url
                else:
                    quit(1)
            else:
                quit(1)
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


if __name__ == '__main__':
    main()
