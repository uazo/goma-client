@echo off

REM Copyright 2012 The Goma Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.

SET PY=vpython
WHERE %PY% >NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
    SET PY=python
    ECHO vpython not found, fallback to python. This may have some issue ^
finding the necessary modules ^(e.g. pywin32^). Consider installing ^
depot_tools to get vpython.
)

%PY% "%~dp0goma_ctl.py" %*
