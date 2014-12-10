SUBDIRS = spyview debian

#Manual rules to install colormap files
install-data-local:
	$(INSTALL) -d $(DESTDIR)/$(datadir)/cmaps/transform
	$(INSTALL) -d $(DESTDIR)/$(datadir)/cmaps/nanoscope
	$(INSTALL) -d $(DESTDIR)/$(datadir)/cmaps/wsxm
	$(INSTALL) -d $(DESTDIR)/$(datadir)/cmaps/user

	/bin/cp -r cmaps/*.ppm $(DESTDIR)/$(datadir)/cmaps 
	/bin/cp -r cmaps/transform/*.ppm $(DESTDIR)/$(datadir)/cmaps/transform 
	/bin/cp -r cmaps/nanoscope/ns??.ppm $(DESTDIR)/$(datadir)/cmaps/nanoscope 
	/bin/cp -r cmaps/wsxm/*.ppm $(DESTDIR)/$(datadir)/cmaps/wsxm 
	$(INSTALL_DATA) spyview/default_image.pgm $(DESTDIR)/$(datadir)
