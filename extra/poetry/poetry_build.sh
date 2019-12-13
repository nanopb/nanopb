#!/bin/bash

rm -rf nanopb
mkdir nanopb
touch nanopb/__init__.py
cp -pr ../../generator nanopb/
cp -pr ../../README.md .
sed -i -e 's/\(version =.*\)-dev.*/\1-dev+'$(git rev-parse --short HEAD)'"/' pyproject.toml
poetry build
