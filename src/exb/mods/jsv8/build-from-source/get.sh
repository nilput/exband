function fail() {
    echo 'An error occured' >&2
    exit 1
}
mkdir -p archives
cd archives || fail
if ! [ -d depot_tools ]; then
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
fi
if ! [ -d v8 ]; then
    git clone https://github.com/v8/v8.git
fi
