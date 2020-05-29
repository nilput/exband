#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
export PATH="$PATH:""$script_dir""/../garbage/apib/build/apib/"
apib -c 1000 http://127.0.0.1:8085 -d 10
