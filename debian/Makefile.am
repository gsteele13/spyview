

if MAKEPDF

if WINDOWS
docdir=${prefix}/bin
else
docdir=${prefix}/share/doc/@PACKAGE@/
endif
doc_DATA=dat2pgm.pdf  pm3d2dat.pdf  splitfield.pdf  spybrowse.pdf  spyview.pdf  spyview3d.pdf

%.pdf: %.1
	@GROFF@ -t -e -mandoc -Tps $^ | @PSTOPDF@ - - > $@

endif