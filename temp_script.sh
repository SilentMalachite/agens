#!/bin/bash
output=$(AGENS_LANG=en AGENS_UNIFIED_GPU_RATIO=0.3 ./build/agens -p hi 2>&1)
echo "OUTPUT: $output"
echo exit code: $?
