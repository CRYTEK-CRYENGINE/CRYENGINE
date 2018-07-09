# CRYENGINE
This repository houses the source code for CRYENGINE.

Instructions on getting started with git can be found [here](http://docs.cryengine.com/display/CEPROG/Getting+Started+with+git), along with details on working with launcher projects and git source code.


## Building
In order to compile, you will need to download some third party SDKs. They can be downloaded by running the *download_sdks.py* script.
Or on Windows, the *download_sdks.exe* can be used alternatively.

CMake must be used to compile the engine, see [here](http://docs.cryengine.com/display/CEPROG/CMake) for more information.

In order to build Sandbox, the FBX SDK (2016.1) must be downloaded from [Autodesk](http://www.autodesk.com).


## Branches
Development takes place primarily in the "main" branch. The stabilisation branch is used for fixing bugs in the run-up to release, and the release branch provides stable snapshots of the engine.

To prepare for a major (feature) release, we integrate "main" into "stabilisation", and then continue fixing bugs in "stabilisation". To prepare for a minor (stability) release, individual changes from 'main are integrated directly into "stabilisation". In each case, when the release is deemed ready, "stabilisation" is integrated to "release".

Pull requests can only be accepted into the "pullrequests" branch. Thanks in advance!


## License
The source code in this repository is governed by the CRYENGINE license agreement, which is contained in [LICENSE.md](LICENSE.md), adjacent to this file. See also the FAQ [here](FAQ.md)

```diff
+ Please note: from March 20th 2018, the new CRYENGINE business model is in effect. 
+ That means 5% royalties apply to projects developed and published on CRYENGINE 5.0 and beyond. 
+ Check our new FAQ.md for all facts and exemptions.
```
