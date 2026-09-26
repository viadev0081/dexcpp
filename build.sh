SOURCES=(
    src/main.cpp
    src/lib/dexlib.cpp
)

echo "[Building DexCPP editor]"
echo "[BUILD]: clang++ main.cpp -o main" &&  clang++ "${SOURCES[@]}" -o main
echo "[BUILD]: chmod +x ./main"
echo "[RUN]: ./main" && ./main
