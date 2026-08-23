@echo off
:: This file is used to invoke nanopb_validate_generator.py as a plugin
:: to protoc on Windows.
:: Use it like this:
:: protoc --plugin=protoc-gen-nanopb-validate=..../protoc-gen-nanopb-validate.bat ^
::        --nanopb_out=--protoc-insertion-points:dir ^
::        --nanopb-validate_out=dir foo.proto
::
:: Note that --nanopb_out must come first: this plugin injects into the files
:: that nanopb generates, and protoc runs generators in command line order.

set mydir=%~dp0
python "%mydir%\nanopb_validate_generator.py" --protoc-plugin %*
