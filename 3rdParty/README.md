To build `cppdap` which is required for the LuaVSCodeDebugAdapter tool:
1. clone `cppdap1 into this folder using the command
    ```
    git clone https://github.com/google/cppdap
    ```
2. configure, build debug and release
    ```
    cd cppdap
    mkdir build
    cd build
    cmake ..
    ```
    Open the Visual Studio solution (Windows) or use `make` to build (Linux/macOS) debug and release libraries
3. after building, build the debug and release targets with the `CMAKE_INSTALL_PREFIX` set to `/path/to/gem/3rdParty/cppdap/install` or modify the `Findcppdap.cmake` file to look in the correct folder (currently set to `cppdap/install`) for the headers and libraries.