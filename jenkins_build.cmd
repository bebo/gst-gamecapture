@ECHO ON
set errorlevel=
set TEMP_FILENAME=%TEMP%\%JOB_NAME%_%TAG%.zip
set FILENAME=%JOB_NAME%_%TAG%.zip

rmdir /s /q dist
rmdir /s /q x64
rmdir /s /q Release

pushd .
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvars64.bat" amd64
popd
MSBuild.exe /property:Configuration=Release /property:Platform=x64 /target:clean,build

if errorlevel 1 (
  echo "Build Failed with %errorlevel%"
  exit /b %errorlevel%
)

mkdir dist
if errorlevel 1 (
  echo "mkdir dist failed with %errorlevel%"
  exit /b %errorlevel%
)

xcopy x64\Release\*.dll dist\

if errorlevel 1 (
  echo "Failed xcopy x64\Release\*.dll dist\ with %errorlevel%"
  exit /b %errorlevel%
)

xcopy x64\Release\*.pdb dist\
if errorlevel 1 (
    echo "Failed xcopy x64\Release\*.pdb dist\ with %errorlevel%"
    exit /b %errorlevel%
)

xcopy third_party\obs-binaries\* dist\
if errorlevel 1 (
    echo "Failed third_party\obs-binaries\* dist\ with %errorlevel%"
    exit /b %errorlevel%
)

"C:\Program Files\7-Zip\7z.exe" a -r %TEMP_FILENAME% -w .\dist\* -mem=AES256

@if errorlevel 1 (
    echo "zip failed with %errorlevel%"
    exit /b %errorlevel%
)

"C:\Program Files\Amazon\AWSCLI\aws.exe" s3api put-object --bucket bebo-app --key repo/%JOB_NAME%/%FILENAME% --body %TEMP_FILENAME%
@if errorlevel 1 (
    echo "failed to upload to s3 with %errorlevel%"
    exit /b %errorlevel%
)
@echo "Uploaded to %JOB_NAME%/%FILENAME%"
