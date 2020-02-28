function fail() {
    echo 'An error occured' >&2
    exit 1
}

fname='Python-3.7.2.tgz'
if ! [ -f archives/"$fname" ]; then
    if ! cd archives; then
        mkdir archives || fail
    fi  
    cd archives || fail
    wget https://www.python.org/ftp/python/3.7.2/Python-3.7.2.tgz || fail
fi
