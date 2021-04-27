#!/bin/bash

group=dla
priority=85
mem=50000
ncpus=2

for dir in "$@"; do
  (
    cd "$dir";
    if [ -e "arc-job-id.txt" ]; then
      echo "Skipping ${dir} as it has already been run"
    else
      echo "Starting build in ${dir}..."
      arc submit --farm group=$group priority=$priority mem=$mem ncpus=$ncpus -- ./build.sh >arc-job-id.txt
    fi
  );
done
