#!/bin/bash

rm -rf nanopb
mkdir nanopb
touch nanopb/__init__.py
cp -pr ../../generator nanopb/
cp -pr ../../README.md .
poetry build
