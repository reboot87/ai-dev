# Build
mkdir build && cd build
cmake .. -G "MinGW Makefiles"   # Windows MinGW
cmake ..                         # Linux
cmake --build .

# Run
./threadpool_cpp03
./threadpool_cpp11
./threadpool_cpp17
./threadpool_c
