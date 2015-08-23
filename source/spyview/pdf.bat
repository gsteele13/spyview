set gs="c:\Program Files\gs\gs8.54\bin\gswin32c.exe"
set out=%1%.png
set opts=-dAutoFilterColorImages=false -dAutoFilterGrayImages=false -dColorImageFilter=/FlateEncode -dGrayImageFilter=/FlateEncode
echo %opts%
%gs% -q -sPAPERSIZE=a4 -sDEVICE=pdfwrite -sOutputFile=%out% -dBATCH -dNOPAUSE %opts% %1%
