call "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
bash ./scripts/configure.sh release
bash ./scripts/build.sh release install
