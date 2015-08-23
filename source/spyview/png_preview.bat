set gs="c:\Program Files\gs\gs8.54\bin\gswin32c.exe"
set out=%1%.png
echo %opts%
%gs% -q -r150x150 -sPAPERSIZE=a4 -sDEVICE=pngalpha -sOutputFile=%out% -dBATCH -dNOPAUSE %opts% %1%
start %out%
