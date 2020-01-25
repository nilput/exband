#!/bin/bash
script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
"$script_dir"/../garbage/apib/bin/apib -c 2000 http://127.0.0.1:8085/ -d 300
