@echo off
:: Copy the file without prompting to overwrite it, and then 'touch' the
:: output file to timestamp it with the current time.
copy /b /y "%1" "%2" >NUL && pushd "%~dp2" && (copy /b "%~nx2"+,, >NUL & popd)