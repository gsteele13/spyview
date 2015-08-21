#!/bin/bash

# Find dependencies with: 'otool -L spyview'
# Change dependencies location with 
# 'install_name_tool -change ../../../mydl/build/Release/libmydl.dylib @executable_path/libmydl.dylib spyview'

install_name_tool -change /usr/lib/libSystem.B.dylib @executable_path/libSystem.B.dylib $1
install_name_tool -change /usr/local/lib/libboost_regex.dylib @executable_path/libboost_regex.dylib $1
install_name_tool -change /usr/local/lib/libboost_serialization.dylib @executable_path/libboost_serialization.dylib $1
install_name_tool -change /usr/local/lib/libfltk_gl.1.3.dylib @executable_path/libfltk_gl.1.3.dylib $1
install_name_tool -change /usr/local/lib/libfltk_images.1.3.dylib @executable_path/libfltk_images.1.3.dylib $1
install_name_tool -change /usr/local/lib/libfltk.1.3.dylib @executable_path/libfltk.1.3.dylib $1
install_name_tool -change /usr/local/lib/libpng16.16.dylib @executable_path/libpng16.16.dylib $1
install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/libz.1.dylib $1
install_name_tool -change /opt/local/lib/libXext.6.dylib @executable_path/libXext.6.dylib $1
install_name_tool -change /opt/local/lib/libX11.6.dylib @executable_path/libX11.6.dylib $1
install_name_tool -change /usr/lib/libc++.1.dylib @executable_path/libc++.1.dylib $1
