gclient config https://chromium.googlesource.com/v8/v8.git
cd archives/v8
git add -A
git reset --hard
git checkout 8.1.307.20
gclient sync
cd ../../
mkdir -p build
cd build
fetch v8
cd v8
./build/install-build-deps.sh
./tools/dev/gm.py x64.release
