#!/usr/bin/env bash

shopt -s globstar
clang-format -i main/**/*.{h,c} components/**/*.{h,c}
python -m kconfcheck main/**/Kconfig.projbuild components/**/Kconfig.projbuild
