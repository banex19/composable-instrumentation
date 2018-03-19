# composable-instrumentation
A proof of concept of an instrumentation framework that supports composition.

To build:
```
    1) git clone https://github.com/banex19/composable-instrumentation.git
    2) cd composable-instrumentation
    3) mkdir build
    4) cd build
    5) LLVM_DIR=/path/to/llvm/cmake cmake ..
    6) make 
```

To run the examples after the instrumentation pass is built:
```
    1) cd composable-instrumentation
    2) cd Tests
    3) (optional) chmod +x test.sh
    3) ./test.sh
```
