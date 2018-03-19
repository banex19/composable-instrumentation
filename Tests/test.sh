#!/bin/sh

RED='\033[0;31m'
NC='\033[0m'

echo  "${RED}Only tool instrumement.c\n${NC}"
clang++ -Wno-deprecated -Xclang -load  -Xclang ./../build/InstrumentPass/libInstrumentPass.so -mllvm "-instrument-tools" -mllvm "instrument.c" test.c instrument.c 
./a.out

echo  "\n${RED}Only tool time.cpp\n${NC}"
clang++ -Wno-deprecated -Xclang -load  -Xclang ./../build/InstrumentPass/libInstrumentPass.so -mllvm "-instrument-tools" -mllvm "time.cpp" test.c time.cpp
./a.out

echo  "\n${RED}Both instrument.c and time.cpp - multiplexed for target program and no tool composing\n${NC}"
clang++ -Wno-deprecated -Xclang -load  -Xclang ./../build/InstrumentPass/libInstrumentPass.so -mllvm "-instrument-tools" -mllvm "instrument.c" -mllvm "-instrument-tools" -mllvm "time.cpp" -mllvm "-target-instrumentation" -mllvm "Multiplex" test.c instrument.c time.cpp
./a.out

echo  "\n${RED}Both instrument.c and time.cpp - only the first tool used for target program and chained composition for tools\n${NC}"
clang++ -Wno-deprecated -Xclang -load  -Xclang ./../build/InstrumentPass/libInstrumentPass.so -mllvm "-instrument-tools" -mllvm "instrument.c" -mllvm "-instrument-tools" -mllvm "time.cpp" -mllvm "-target-instrumentation" -mllvm "FirstToolOnly"  -mllvm "-compose-function" -mllvm "Chained"  test.c instrument.c time.cpp
./a.out


rm a.out
