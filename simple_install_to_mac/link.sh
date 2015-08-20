#!/bin/bash

ln -s bin/spyview /usr/local/bin
cd dylibs
ln -s libSystem.B.dylib usr/lib/libSystem.B.dylib 
ln -s libboost_regex.dylib /usr/local/lib/libboost_regex.dylib 
ln -s libboost_serialization.dylib /usr/local/lib/libboost_serialization.dylib 
ln -s libfltk_gl.1.3.dylib /usr/local/lib/libfltk_gl.1.3.dylib 
ln -s libfltk_images.1.3.dylib /usr/local/lib/libfltk_images.1.3.dylib 
ln -s libfltk.1.3.dylib /usr/local/lib/libfltk.1.3.dylib 
ln -s libpng16.16.dylib /usr/local/lib/libpng16.16.dylib 
ln -s libz.1.dylib /opt/local/lib/libz.1.dylib 
ln -s libXext.6.dylib /opt/local/lib/libXext.6.dylib 
ln -s libX11.6.dylib /opt/local/lib/libX11.6.dylib 
ln -s libc++.1.dylib /usr/lib/libc++.1.dylib
cd ..
