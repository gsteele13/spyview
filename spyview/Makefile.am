AM_CPPFLAGS = -DBUILDSTAMP="\"$(shell hostname) $(shell date)\""


bin_PROGRAMS = spyview spybrowse dat2pgm mtxextract stminfo grid_data test_imagedata @WIN32_EXTRA@ gp_to_mat dat2mtx mtxdiff huettel2mtx toeno2mtx spypal gilles2mtx ns2pgm
#bin_PROGRAMS += spyview3d
#bin_PROGRAMS = spyview @WIN32_EXTRA@
#bin_SCRIPTS = splitfield pm3d2dat gp_to_mat.sh spyrot spyrot_comment pnm_comment pnm_comments
EXTRA_PROGRAMS = spyview_console spybrowse_console

gp_to_mat_SOURCES = gp_to_mat.C

ns2pgm_SOURCES = ns2pgm.c
ns2pgm_LDFLAGS = @WIN32_CONSOLE_FLAGS@

spypal_SOURCES=spypal.C spypal_wizard.C cclass.C spypal_gradient.C Fl_Table.C message.C spypal_interface.C FLTK_Serialization.C  spypal_import.C
spypal_CXXFLAGS=-DSPYPAL_STANDALONE

spyview_SOURCES = spyview_ui.C spyview.C \
		ImageWindow.C \
		ImageData.C \
		ImageWindow_Module.C \
		Gnuplot_Interface.C \
		message.C \
		ImagePrinter.C ImagePrinter_Control.C ImagePrinter_Control.h \
		ImageWindow_LineDraw.C \
		Fiddle.C \
		PeakFinder.C PeakFinder_Control.C PeakFinder_Control.h\
		ImageWindow_Fitting.C ImageWindow_Fitting_Ui.C \
		ThresholdDisplay.C ThresholdDisplay_Control.C \
	        LineDraw_Control.h LineDraw_Control.C \
		misc.C \
                spypal.C spypal_wizard.C cclass.C\
		eng.C spypal_gradient.C Fl_Table.C spypal_interface.C FLTK_Serialization.C spypal_import.C 

imagedata_code = ImageData.H ImageData.C message.C misc.C

test_imagedata_SOURCES = test_imagedata.C $(imagedata_code)

spyview_console_SOURCES = $(spyview_SOURCES)
spyview_console_LDFLAGS = @WIN32_CONSOLE_FLAGS@

spybrowse_console_SOURCES = $(spybrowse_SOURCES)
spybrowse_console_LDFLAGS = @WIN32_CONSOLE_FLAGS@

spybrowse_SOURCES = spybrowse_ui.fl spybrowse.C spybrowse.h message.C \
			spybrowse_ui.C spybrowse_ui.h Fl_Table.C Fl_Listbox.C
#spyview3d_SOURCES = spyview3d_ui.fl spyview3d.C ImageWindow3d.C ImageWindow3d.H message.C \
#			gl2ps.c gl2ps.h Image2D.C Image2D.H  \
#			spyview3d.H spyview3d_ui.C spyview3d_ui.h
#spyrotate_SOURCES = spyrotate_ui.fl spyrotate.C ImageWindow.C ImageWindow.H message.C \
#			Image2D.C Image2D.H ImagePrinter.C\
#			spyrotate.h spyrotate_ui.C spyrotate_ui.h\
#			ImagePrinter_Control.C ImageWindow_LineDraw.C ImageData.H ImageDatea.C \
#			PeakFinder.C PeakFinder_Control.C PeakFinder_Control.h Gnuplot_Interface.C \
#			ImageWindow_Fitting.C ImageWindow_Fitting_Ui.C ImageWindow_Module.C
#spynavigate_SOURCES = spynavigate_ui.fl spynavigate.C ImageWindow.C ImageWindow.H message.C \
#			Image2D.C Image2D.H ImagePrinter.C\
#			spynavigate.h spynavigate_ui.C spynavigate_ui.h\
#			ImagePrinter_Control.C ImageWindow_LineDraw.C ImageData.H ImageData.C \
#			PeakFinder.C PeakFinder_Control.C PeakFinder_Control.h Gnuplot_Interface.C \
#			ImageWindow_Fitting.C ImageWindow_Fitting_Ui.C ImageWindow_Module.C

dat2mtx_SOURCES = dat2mtx.C $(imagedata_code)
dat2mtx_LDFLAGS = @WIN32_CONSOLE_FLAGS@
huettel2mtx_SOURCES = huettel2mtx.C $(imagedata_code)
huettel2mtx_LDFLAGS = @WIN32_CONSOLE_FLAGS@
toeno2mtx_SOURCES = toeno2mtx.C $(imagedata_code)
toeno2mtx_LDFLAGS = @WIN32_CONSOLE_FLAGS@
gilles2mtx_SOURCES = gilles2mtx.C $(imagedata_code)
gilles2mtx_LDFLAGS = @WIN32_CONSOLE_FLAGS@
mtxdiff_SOURCES = mtxdiff.C $(imagedata_code)
mtxdiff_LDFLAGS = @WIN32_CONSOLE_FLAGS@
dat2pgm_SOURCES = dat2pgm.C
dat2pgm_LDFLAGS = @WIN32_CONSOLE_FLAGS@
stminfo_SOURCES = stminfo.C
stminfo_LDFLAGS = @WIN32_CONSOLE_FLAGS@
mtxextract_SOURCES = mtxextract.C
mtxextract_LDFLAGS = @WIN32_CONSOLE_FLAGS@
grid_data_SOURCES = grid_data.C

BUILT_SOURCES = spyview_ui.C spyview_ui.h \
		spybrowse_ui.h spybrowse_ui.C \
		spyview3d_ui.C spyview3d_ui.h \
		spyrotate_ui.C spyrotate_ui.h \
		spynavigate_ui.C spynavigate_ui.h \
		ImagePrinter_Control.C ImagePrinter_Control.h \
		PeakFinder_Control.C PeakFinder_Control.h \
		ImageWindow_Fitting_Ui.C ImageWindow_Fitting_Ui.h \
		ThresholdDisplay_Control.C ThresholdDisplay_Control.h \
		LineDraw_Control.C LineDraw_Control.h \
		spypal_wizard.C spypal_wizard.h 

# Fluid rules
%.C: %.fl
	@FLUID@ -c -o $(basename $<).C -h $(basename $<).h $<

%.h: %.fl
	@FLUID@ -c -o $(basename $<).C -h $(basename $<).h $<

#For the windows icon

.rc.o:
	i586-mingw32msvc-windres $^ -o $@
%.o : %.rc
	i586-mingw32msvc-windres $^ -o $@

#Manual rules to install colormap files
install-data-local:
	$(INSTALL) -d $(DESTDIR)/$(datadir)
	$(INSTALL_DATA) help.txt $(DESTDIR)/$(datadir)
	$(INSTALL) -d $(DESTDIR)/$(datadir)/html
	$(INSTALL_DATA) html/*.html $(DESTDIR)/$(datadir)/html
