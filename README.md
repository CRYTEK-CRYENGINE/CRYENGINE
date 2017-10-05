# CRYENGINE
This repository houses the source code for CRYENGINE.

Instructions on getting started with Git can be found [here](http://docs.cryengine.com/display/CEPROG/Getting+Started+with+git), along with details on working with launcher projects and git source code.

TABLE OF CONTENTS
=================

* [Building](*building) 
* [Branches](*branches)
* [License](*license)

## Building
In order to compile, you will need to download some third-party SDKs. They can be downloaded by running the *download_sdks.py* script.
Or on windows, the *download_sdks.exe* can be used alternatively.

CMake must be used to compile the engine, see [here](http://docs.cryengine.com/display/CEPROG/CMake) for more information.


## Branches
Development takes place primarily in the "main" branch. The stabilization branch is used for fixing bugs in the run-up to release, and the release branch provides stable snapshots of the engine.

To prepare for a major (feature) release, we integrate "main" into "stabilization", and then continue fixing bugs in "stabilization". To prepare for a minor (stability) release, individual changes from 'main are integrated directly into "stabilization". In each case, when the release is deemed ready, "stabilization" is integrated to "release".

Pull requests can only be accepted into the "pull requests" branch.


## License
The source code in this repository is governed by the CryEngine license agreement, which is contained in LICENSE.md, adjacent to this file.
