# CRYENGINE
This repository houses the source code for CRYENGINE.

Instructions on getting started with git can be found [here](http://docs.cryengine.com/display/CEPROG/Getting+Started+with+git), along with details on working with launcher projects and git source code.


## Building

In order to compile, you will need to install:
* A Visual Studio 2015, 2017 or 2019
* [FBX SDK (2016.1)](http://images.autodesk.com/adsk/files/fbx20161_fbxsdk_vs2015_win0.exe)

Steps:
1. Download the SDKs zip from the [releases page](https://github.com/CRYTEK/CRYENGINE/releases).
2. Extract the SDKs zip to `Code/Libs`
3. Copy `C:\Program Files\Autodesk\FBX\FBX SDK\2016.1` to `Code/Libs` and rename to `FbxSdk`.
4. Download [szip.tar.gz](https://support.hdfgroup.org/ftp/lib-external/szip/2.1.1/src/szip-2.1.1.tar.gz) and extract it's contents to `Code/Libs/szip`.
5. If the CRYENGINE Launcher is installed, right click `cryengine.cryengine` and select "Generate Engine Solution". Otherwise, run `Tools/CMake/cmake_create_win64_solution.bat`.

CMake must be used to compile the engine, see [here](http://docs.cryengine.com/display/CEPROG/CMake) for more information.

## Branches
The `release` branch provides stable snapshots of the engine. Most of the commits to this branch are [tagged](https://github.com/CRYTEK/CRYENGINE/releases).

The `stabilisation` branch is used for fixing bugs in the run-up to release. This branch is created by branching from `main` or the previous release, depending if it's a major (feature) or minor (stability) release.

A `release_candidate` branch may be created for a few days before a release, if we need some critical fixes before release.

Development takes place primarily in the `main` branch. This branch is not currently available for the general public.

## Pull Requests
Pull requests are currently on hold, while we fix all the workflow issues the current process had.
For more details, see this [announcement post](https://www.cryengine.com/news/cryengine-on-github-live-updates-to-main-will-go-on-hiatus). Sorry for the inconvenience.


## License
The source code in this repository is governed by the CRYENGINE license agreement, which is contained in [LICENSE.md](LICENSE.md), adjacent to this file. See also the FAQ [here](FAQ.md)

```diff
+ Please note: from March 20th 2018, the new CRYENGINE business model is in effect. 
+ That means 5% royalties apply to projects developed and published on CRYENGINE 5.0 and beyond. 
+ Check our new FAQ.md for all facts and exemptions.
```
