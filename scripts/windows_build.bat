call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
bash ./scripts/configure.sh %1
bash ./scripts/build.sh %1
