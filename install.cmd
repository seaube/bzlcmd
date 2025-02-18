@echo off

bazel build //bzlmod //bzlreg -c dbg
xcopy /Y /F "%~dp0bazel-bin\bzlmod\bzlmod.exe" "%USERPROFILE%\.local\bin\"
xcopy /Y /F "%~dp0bazel-bin\bzlreg\bzlreg.exe" "%USERPROFILE%\.local\bin\"
