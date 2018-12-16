#!/usr/bin/env python3
## Using Python 3.5
from zipfile import ZipFile
from tarfile import TarFile
from urllib.request import urlopen, Request
import json

import os
import sys
import platform

## Slightly different input/output here.
CMAKE_GUI = False if (len(sys.argv) > 1 or sys.stdout.isatty()) else True
last_perc = -1
def print_progress(current, maxprogress):
    global last_perc
    percent = int(current * 100. / maxprogress)
    if percent != last_perc or not CMAKE_GUI: 
        status = "[%2d%%]  %-10d" % (percent, current)
        print(status, end='\r')
        last_perc = percent

def read_chunks(url, block_sz=8192 * 8):
    with urlopen(url) as openurl:
        while True:
            data = openurl.read(block_sz)
            if data:
                yield data
            else:
                return

def get_json(url):
    r = Request(url)
    #r.add_header('Authorization', 'token f850bf1cfb9aae43c039681db68f6f79cabd141c') ## Debug only
    r = urlopen(r)
    return json.loads(r.read().decode(r.info().get_param('charset') or 'utf-8'))

def alias(path):
    return path.replace(CRYENGINE_DIR, "%ENGINE_ROOT%")
    
## If this python script is in its proper location post git setup, then engine root is just 3 folders away.
## Otherwise default to the directory that script was run from.
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
if (SCRIPT_DIR.endswith("Tools\CMake\download_sdks")):
    CRYENGINE_DIR = os.path.abspath(os.path.join(SCRIPT_DIR,'../../../'))
else:
    CRYENGINE_DIR = os.getcwd()
    
def main():
    print('Detected engine root as: "%s"' % CRYENGINE_DIR)

    ## Load vars from config: User/Repo, release tag, SDK asset name, pre-existing SDK (if any)
    with open(os.path.join(SCRIPT_DIR, "sdk_config.json")) as config:
        globals().update(json.load(config))
    
    # Use ZIP on windows and tar on other platforms
    zip_name = asset
    if platform.system() == 'Windows':
        zip_name +='.zip'
        ArchiveFile = ZipFile
        list_archive = ZipFile.namelist
    else:
        zip_name +='.tar.gz'
        ArchiveFile = TarFile.open
        list_archive = TarFile.getnames
            
    ## Use path to existing zip, if set. Relative to engine root.
    if sdk_dir != "":
        temp_file = os.path.abspath(os.path.join(CRYENGINE_DIR, sdk_dir, zip_name))
    else:
        temp_file = os.path.join(SCRIPT_DIR, zip_name)
    
    if os.path.isfile(temp_file):
        print('Using pre-downloaded SDKs: "%s"' % alias(temp_file))
    else:
        url = get_download_url(repo, rtag, zip_name)
        print('Download URL: "%s"' % url)

        u = urlopen(url)
        meta = u.info()
        file_size = int(u.getheader("Content-Length"))

        print("Downloading %s Bytes" % file_size)
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
        output_path = os.path.join(CRYENGINE_DIR, 'Code', 'SDKs')

        print('Extracting %d files to: "%s"' % (num_files, alias(output_path)))

        for counter, item in enumerate(nameList, start=1):
            zf.extract(item, output_path)
            print_progress(counter, num_files)

    print("Finished downloading/extracting SDKs.")
    
    ## Delete if temporary file
    if sdk_dir == "":
        os.remove(temp_file)

def get_download_url(repo, rtag, zip_name):
    ## Base URL
    URL = "https://api.github.com/repos/"+repo

    ## Make sure repo exists (yea lots of these make-sure checks)
    try:
        json = get_json(URL)
    except:
       raise ValueError('Invalid repo! "%s"' % repo)
        
    ## Get tags
    json = get_json(URL+'/tags')
    
    ## If latest, tag points to same commit as another release tag.
    if("latest" in rtag):
  
        ## Use tag's commit to get the 2'nd tag that points to release.
        com=""
        for t in json:
            if (t['name'] == rtag):
                com=t['commit']['sha']
                break

        if(com != ""):
            for t in json:
                if (t['name'] != rtag and t['commit']['sha'] == com):
                    rtag=t['name']
                    break
        else:
            raise ValueError('No other tag points to the same commit as "%s"' % rtag)
    
    ## Make sure release exists ofc
    try:
        json = get_json(URL+'/releases/tags/'+rtag)
    except:
        raise ValueError('Paired tag "%s" has no release!' % rtag)
    
    ## And that release has the target asset
    URL=""
    for asset in json['assets']:
        if zip_name == asset['name']:
            URL=asset['browser_download_url']
            break

    if URL == "":
        raise ValueError('No asset "%s" found for release tag "%s"' % (zip_name, rtag))
        
    return URL
    
if __name__== '__main__':
    main()
    
    
    