@echo on
erase  svnVersion.svnInc.tmp
SubWcRev \slang svnVersionTemplate.h svnVersion.svnInc.tmp -d
if not exist svnVersion.svnInc goto copy
fc svnVersion.svnInc svnVersion.svnInc.tmp
if %ERRORLEVEL% equ 0 goto noCopy
:copy
copy svnVersion.svnInc.tmp svnVersion.svnInc /y
:noCopy

