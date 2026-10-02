echo "[Building DexCPP editor]"
clang++ -std=c++11 src/main.cpp src/lib/dexlib.cpp -o main -lncursesw
chmod +x ./main
./main
