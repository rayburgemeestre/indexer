#!/bin/ksh93

set -o verbose

find / -printf "%k\t%i\t%A+\t%Y\t%p\n" > index.txt_unsorted
sort --parallel 4 -n index.txt_unsorted > index.txt
rm -rf index.txt_unsorted

sendmsg index ready

