@echo off
@echo Make sure that all copies of spyview are closed (otherwise, windows will not let you overwrite the .exe files...)
pause
xcopy /y *.exe old_version
xcopy /y *.bat old_version
@echo getting files
wget -e robots=off -nv -nd --no-parent -N -A .exe,.bat,.txt -r -l1 http://kavli.nano.tudelft.nl/~gsteele/spyview/windows_exe/
@echo setting permissions
for %%f in (*.exe *.bat) do (
cacls %%f /e /g "Everyone":R
)
pause
