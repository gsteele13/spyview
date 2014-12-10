set -x
# remote=qt.tn.tudelft.nl
# exe="spybrowse.exe dat2mtx.exe spyview.exe spyview_console.exe spybrowse_console.exe mtxdiff.exe huettel2mtx.exe toeno2mtx.exe *.bat pgnuplot.exe wgnuplot.exe gilles2mtx.exe help.txt updates_page.url ns2pgm.exe"
# cp $exe spyview_windows
# scp $exe $remote:public_html/spyview/windows_exe
# rm spyview_windows.zip
# zip -r spyview_windows.zip spyview_windows
# #(cd spyview_windows; zip -r ../spyview_windows.zip .)
# scp spyview_windows.zip $remote:public_html/spyview/
# scp spyview_console.exe help.txt $remote:public_html/spyview/

# Now we're building on the local machine
exe="spybrowse.exe dat2mtx.exe spyview.exe spyview_console.exe spybrowse_console.exe mtxdiff.exe huettel2mtx.exe toeno2mtx.exe *.bat gilles2mtx.exe help.txt updates_page.url ns2pgm.exe"
cp $exe spyview_windows
cp ~/public_html/spyview/windows_exe
rm spyview_windows.zip
zip -r spyview_windows.zip spyview_windows
#(cd spyview_windows; zip -r ../spyview_windows.zip .)
cp spyview_windows.zip ~/public_html/spyview/
cp spyview_console.exe help.txt ~/public_html/spyview/
date > ~/public_html/spyview/update.html
