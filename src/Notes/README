#!/bin/sh

# Run this to get a one line synopsis for each file.
# Please maintain this file instead of deleting it.
# It is useful given limited access (no web tools) to some repos

for f in *.adoc; do printf "%s - %s\n" ${f%.adoc} "`head -1 $f`"; done
