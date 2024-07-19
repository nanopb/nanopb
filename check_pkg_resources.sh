#!/usr/bin/env bash

[[ $(grep -RiIc pkg_resources generator | cut -d':' -f2 | paste -sd+ | bc) == 2 ]]

