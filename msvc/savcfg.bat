@ECHO OFF
REM Get the values of the environment variables which define the build
REM configuration into the file `%CONF_FILE%`.
REM
REM If this file does not exist or the new configuration is different
REM than the one currently on record, delete all object files and update
REM `%CONF_FILE%`.
REM
REM If the recorded `%CONF_FILE%` matches the values in the
REM environment, do nothing.

REM ECHO Checking configuration ...
REM The files we read and write:
SET "CONF_FILE=build_env.bat"
SET "TMP_FILE=%TMP%\NEW_CONFIG"

REM Improvise some commands we need:
REM (The caret `^` prevents that output is redirected *right here*.)
SET RM=DEL /Q ^>NUL: 2^>^&1
SET LS=DIR /B/L 2^>NUL:
SET CP=COPY /Y ^>NUL: 
SET WRTMP=ECHO^>^>"%TMP_FILE%"
SET WRNL=ECHO.^>^>"%TMP_FILE%"

REM First write the current environment values into the temporary file.
REM The format doesn't matter, we write a batch script format that
REM could reproduce the values.
REM All values (except the configuration name) may contain spaces!

%RM% "%TMP_FILE%"
ECHO REM Build configuration:> "%TMP_FILE%"
%WRNL%
%WRTMP%   SET PROJECT=%PROJECT%
%WRTMP%   SET CONF=%CONF%
%WRTMP%   SET CFLAGS=%CFLAGS%
%WRTMP%   SET AFLAGS=%AFLAGS%
%WRNL%
%WRTMP%   SET DEFINES=%DEFINES%
%WRNL%
%WRTMP%   SET LIBS=%LIBS%
%WRNL%
%WRTMP%   SET INCLUDE=%INCLUDE%
%WRNL%
%WRTMP%   SET LIB=%LIB%

REM Compare the `%CONF_FILE%` with the temporary file. If the
REM files are identical, we're done.

ECHO N | COMP "%TMP_FILE%" "%CONF_FILE%" >NUL: 2>&1
IF ERRORLEVEL 1 (GOTO SETNEW) 
REM ECHO Configuration unchanged.
EXIT 0

:SETNEW

REM Otherwise, the new configuration is different, and the object files
REM are no longer valid. Delete them all, then replace the old
REM `%CONF_FILE%` with the temporary file that has the current
REM values.

%CP% "%TMP_FILE%" "%CONF_FILE%"

REM ECHO Configuration changed.
ECHO.
ECHO.
ECHO ************ BUILD CONFIGURATION ************
TYPE "%CONF_FILE%"
ECHO *********************************************
ECHO.
ECHO.

ECHO Deleting object files and targets: (
%LS% *.obj %*
%RM% *.obj %*
ECHO )
EXIT 1

REM EOF ****************************************************************
REM vim:noet:ts=8:sw=8:tw=0
