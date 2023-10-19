#!/usr/bin/env python3
# This script is a wrapper to invoke nanopb_generator from an installed Python module.
import sys
from nanopb.generator.nanopb_generator import main_cli, main_plugin
if __name__ == '__main__':
    sys.exit(main_cli())
