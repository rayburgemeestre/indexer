#!/bin/ksh93

set -o verbose

find /mnt/ -printf "%k\t%i\t%A+\t%Y\t%p\n" > index.txt_unsorted
find /root/ -printf "%k\t%i\t%A+\t%Y\t%p\n" >> index.txt_unsorted
find /mnt2/NAS/ -printf "%k\t%i\t%A+\t%Y\t%p\n" >> index.txt_unsorted
sort --parallel 4 -n index.txt_unsorted > index.txt
rm -rf index.txt_unsorted

sendmsg NAS index ready
echo DONE

