set -x

#  problems so far:
#    netpgm installed only as dynlib
#    libjpeg needed for static build (not specified in dynamic?) but only as dynlib and .la
#    -lXft is needed when statically linking fltk? (should be installed with X11)

g++  -arch i386 -Wall -D_GNU_SOURCE -fpermissive  -L/usr/X11/lib -R/usr/X11/lib -L/sw/lib -o spyview  spyview_ui.o spyview.o ImageWindow.o ImageData.o ImageWindow_Module.o Gnuplot_Interface.o message.o ImagePrinter.o ImagePrinter_Control.o ImageWindow_LineDraw.o Fiddle.o PeakFinder.o PeakFinder_Control.o ImageWindow_Fitting.o ImageWindow_Fitting_Ui.o ThresholdDisplay.o ThresholdDisplay_Control.o LineDraw_Control.o misc.o spypal.o spypal_wizard.o cclass.o eng.o spypal_gradient.o Fl_Table.o spypal_interface.o FLTK_Serialization.o spypal_import.o  -lm \
-lnetpbm /sw/lib/libboost_regex.a /sw/lib/libboost_serialization.a \
/sw/lib/libfltk_gl.a /sw/lib/libfltk_images.a /sw/lib/libfltk.a /sw/lib/libpng.a -lz \
-L/usr/X11/lib -R/usr/X11/lib -lXext -lX11 -lXft -ljpeg

#g++  -arch i386 -Wall -D_GNU_SOURCE -fpermissive  -L/usr/X11/lib -R/usr/X11/lib -L/sw/lib -o spyview  spyview_ui.o spyview.o ImageWindow.o ImageData.o ImageWindow_Module.o Gnuplot_Interface.o message.o ImagePrinter.o ImagePrinter_Control.o ImageWindow_LineDraw.o Fiddle.o PeakFinder.o PeakFinder_Control.o ImageWindow_Fitting.o ImageWindow_Fitting_Ui.o ThresholdDisplay.o ThresholdDisplay_Control.o LineDraw_Control.o misc.o spypal.o spypal_wizard.o cclass.o eng.o spypal_gradient.o Fl_Table.o spypal_interface.o FLTK_Serialization.o spypal_import.o  -lm /sw/lib/libnetpbm.a /sw/lib/libboost_regex.a /sw/lib/libboost_serialization.a /sw/lib/libfltk_gl.a /sw/lib/libfltk_images.a /sw/lib/libfltk.a /sw/lib/libpng.a -L/usr/X11/lib -R/usr/X11/lib -lXext -lX11

# For some reason, netpbm is only installed as a dynamic lib
#g++  -arch i386 -Wall -D_GNU_SOURCE -fpermissive  -L/usr/X11/lib -R/usr/X11/lib -L/sw/lib -o spyview  spyview_ui.o spyview.o ImageWindow.o ImageData.o ImageWindow_Module.o Gnuplot_Interface.o message.o ImagePrinter.o ImagePrinter_Control.o ImageWindow_LineDraw.o Fiddle.o PeakFinder.o PeakFinder_Control.o ImageWindow_Fitting.o ImageWindow_Fitting_Ui.o ThresholdDisplay.o ThresholdDisplay_Control.o LineDraw_Control.o misc.o spypal.o spypal_wizard.o cclass.o eng.o spypal_gradient.o Fl_Table.o spypal_interface.o FLTK_Serialization.o spypal_import.o  -lm -lnetpbm /sw/lib/libboost_regex.a /sw/lib/libboost_serialization.a /sw/lib/libfltk_gl.a /sw/lib/libfltk_images.a /sw/lib/libfltk.a /sw/lib/libpng.a -L/usr/X11/lib -R/usr/X11/lib -lXext -lX11

# dat2mtx
#g++  -arch i386 -Wall -D_GNU_SOURCE -fpermissive  -L/usr/X11/lib -R/usr/X11/lib -L/sw/lib -o dat2mtx  dat2mtx.o ImageData.o message.o misc.o  -lm -lnetpbm /sw/lib/libboost_regex.a /sw/lib/libboost_serialization.a /sw/lib/libfltk_gl.a /sw/lib/libfltk_images.a /sw/lib/libfltk.a /sw/lib/libpng.a -lz   -L/usr/X11/lib -R/usr/X11/lib -lXext -lX11
