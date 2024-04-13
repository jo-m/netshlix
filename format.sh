#!/bin/bash

shopt -s globstar
clang-format -i main/**/*.{h,c} rtp/**/*.{h,c}
