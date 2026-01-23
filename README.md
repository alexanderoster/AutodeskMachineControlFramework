# Autodesk Machine Control Framework
The Autodesk Machine Control Framework is a solid, open machine controller interface powered by modern web technologies.

## Current features:
* Extensible C++ Plugin interface 

* Multiple (scripting) language-support is planned (e.g. Python, C++, Pascal, LUA, C#, JavaScript)

* Easy to access, change and configure

* Semantically versioned: multiple machine firmware states can be deployed on the same system and changed on the fly.

* Transactional: record full timeline of machine and replay and debug with millisecond resolution.

* Supports Simulation drivers for Unit-Testing

* Made for Git


## Documentation
TODO


## Dependencies / Prerequisites / Compilation (on Windows)
* Visual Studio C/C++ (the Community Edition is sufficient technically) TODO
* Get and install CMake: https://cmake.org/download/
* Get and install Node.js if you plan to rebuild the client: https://nodejs.org/en/download/
* Run `build_clean_win64.bat` to build the binaries and dev packages. Outputs are in `build_win64/Output`.


## Dependencies / Prerequisites / Compilation (on x64-Linux)
* Get and install Node.js if you plan to rebuild the client: https://nodejs.org/en/download/
* On x64 Debian, the following packages should be sufficient: 
    - build-essential
    - cmake
    - git
    - uuid-dev
    - libssl-dev
* Run `./build_clean_linux64.sh` to build the binaries and dev packages. Outputs are in `build_linux64/Output`.

## Dependencies / Prerequisites (on ARM Linux)
TODO

## Build and first run
Build as described above.
* run `amc_server.exe` on Windows or `./amc_server` on Linux from the output folder
* connect a web browser to http://[your server name or IP]:[your port number]

## Client and API docs packages
* To rebuild the client distribution zip:
  - Windows: run `Artifacts/build_client_clean.bat`
  - Linux: run `./Artifacts/build_client_clean.sh`
  - Output: `Artifacts/clientdist/clientpackage.zip` and `Artifacts/clientdist/clientsourcepackage.zip`
* To rebuild the API docs package:
  - Windows: run `Artifacts/build_apidocs_clean.bat`
  - Linux: run `./Artifacts/build_apidocs_clean.sh`
  - Output: `Artifacts/apidocsdist/apidocspackage.zip`


## Contributing
The Autodesk Machine Control Framework is an open source project.
Contributions are welcome and we are looking for people that can improve existing functionality or create new integrations. Have a look at the [contributor's guide](CONTRIBUTING.md) for details.

## License and 3rd party acknowledgements
* Autodesk Machine Control Framework has a [BSD-3-Clause license](LICENSE.md)
* Autodesk Machine Control Framework uses these [3rd Party components](3RD_PARTY.md)
