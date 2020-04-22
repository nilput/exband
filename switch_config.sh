#!/bin/bash
if [ -z "$1" ]; then
    echo "Usage: $0 [ini]" >&2;
    exit 1;
fi
ln -sf "$1" exb.ini
