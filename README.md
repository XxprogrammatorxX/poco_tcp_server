# POCO TCP server

### Build:
``` 
$ mkdir build && cd build && cmake ..
```

or

```
$ conan install ./conanfile.txt --build=missing
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=./Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
```
### Run:
Server runs on localhost:28888
```
$ ./ReversedEchoServer
```

