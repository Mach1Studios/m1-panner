#!/bin/sh

mkdir universal
for lib in arm64/*; do 
  lipo -create -arch arm64 $lib -arch x86_64 x86_64/$(basename $lib) -output universal/$(basename $lib); 
done
for lib in universal/*; do
    [ -e "$lib" ] || continue
    lipo $lib -info;
    cp -f $lib $1${lib##*/};
done
