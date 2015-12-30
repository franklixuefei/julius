#!/bin/bash

./configure --prefix=`pwd` --enable-fork
make && make install
cp bin/julius ./julius.exe
cd julius-simple
make
mv julius-simple julius.exe
cd -

