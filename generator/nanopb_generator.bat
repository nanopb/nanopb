@echo off
:: Allow calling nanopb_generator.py as simply nanopb_generator.
:: This provides consistency with packages installed through CMake or pip.
set mydir=%~dp0
python "%mydir%\nanopb_generator.py" %*
