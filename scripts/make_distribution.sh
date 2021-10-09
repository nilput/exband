#!/bin/bash
MINIMAL=true
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
exb_dir="$(dirname "$script_dir")"
function die() {
    echo -e $* >&2
    exit 1;
}
cd "$exb_dir" || die "failed to change dir to $exb_dir"
[ -d dist ] && die 'panic: remove ./dist directory first\n'
mkdir dist || die "failed to mkdir"
cp exb obj/libexb.so dist 
if [ "$MINIMAL" = true ]; then
	mkdir dist/examples dist/config || die "failed to mkdirs in dist"
	cp -r examples/basic dist/examples
	cp config/exb.json dist/config
else
	find config -name '*.json' -and -not -type l -exec 'cp' '--parents' '{}' 'dist/' ';'
	find examples -name '*.so' -exec 'cp' '--parents' '{}' 'dist/' ';'
fi
