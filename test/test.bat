@echo OFF

set BZLREG=%~dp0..\bazel-bin\bzlreg\bzlreg.exe
set BZLMOD=%~dp0..\bazel-bin\bzlmod\bzlmod.exe

echo initializing test registry
%BZLREG% init %~dp0reg || exit /b

echo adding rules_cc to test registry
%BZLREG% add-module https://github.com/bazelbuild/rules_cc/releases/download/0.0.8/rules_cc-0.0.8.tar.gz --strip-prefix=rules_cc-0.0.8 --registry=%~dp0reg || exit /b

echo initializing test module
%BZLMOD% init %~dp0module || exit /b

echo common --registry=file://%~dp0reg >> %~dp0module/.bazelrc

cd %~dp0module
%BZLMOD% add rules_cc || exit /b

echo done
