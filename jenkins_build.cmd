ECHO ON
set errorlevel=

rmdir /s /q dist
rmdir /s /q x64
rmdir /s /q Release

pushd .
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars64.bat" amd64
popd
MSBuild.exe /property:Configuration=Release /property:Platform=x64 /target:clean,build

if errorlevel 1 (
  echo Build Failed
  exit /b %errorlevel%
)

mkdir dist
if errorlevel 1 (
  echo Could not mkdir dist
  exit /b %errorlevel%
)

xcopy x64\Release\*.dll dist\
if errorlevel 1 (
  echo Failed xcopy x64\Release\*.dll dist\
  exit /b %errorlevel%
)

xcopy x64\Release\*.pdb dist\
if errorlevel 1 (
  exit /b %errorlevel%
)

xcopy third_party\obs-binaries\* dist\
if errorlevel 1 (
  exit /b %errorlevel%
)

set FILENAME=gst-gamecapture_%TAG%.zip

"C:\Program Files\7-Zip\7z.exe" a -r %FILENAME% -w .\dist\* -mem=AES256

if errorlevel 1 (
  exit /b %errorlevel%
)

"C:\Program Files\Amazon\AWSCLI\aws.exe" s3api put-object --bucket bebo-app --key repo/gst-gamecapture/%FILENAME% --body %FILENAME%
