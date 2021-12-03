@echo off
echo Removing intermediate build files and pre-compiled headers

erase /s *.obj *.sbr *.pch *.pdb *.ncb *.tlog *.sdf *.bsc *.ilk *.log *.cache
erase *.db

echo Done

