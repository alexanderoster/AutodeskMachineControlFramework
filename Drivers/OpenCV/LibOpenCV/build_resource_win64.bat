@ECHO OFF

if exist build_win64 (
  
  echo !!!!!!!!!!!!!!!!!
  echo Build Directory build_win64 already exists. Please ensure a clean checkout first!
  echo !!!!!!!!!!!!!!!!!
  pause
  
) else (
  mkdir build_win64
  cd build_win64
  cmake ..
  cmake --build . --config Release
  pause

)


