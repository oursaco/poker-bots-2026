```bash
mkdir -p build
cd build
cmake ..
make three_card_trainer
cd ..
ulimit -s unlimited
export OMP_PROC_BIND="close" // can do true and spread as well, but close is better
export OMP_PLACES="cores"
export OMP_NUM_THREADS=30 // two less than max number of threads
./build/three_card_trainer
```

```bash
cmake -S . -B build
cmake --build build -j --target three_card_trainer
ulimit -s unlimited
export OMP_PROC_BIND="close" // can do true and spread as well, but close is better
export OMP_PLACES="cores"
export OMP_NUM_THREADS=30 // two less than max number of threads
./build/three_card_trainer
```