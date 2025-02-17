#!/bin/sh -e

# Distributed under the MIT License.
# See LICENSE.txt for details.

@Python_EXECUTABLE@ @CMAKE_SOURCE_DIR@/tools/CleanOutput.py -v --force \
                    --input-file $2 --output-dir @CMAKE_BINARY_DIR@/$3
mkdir @CMAKE_BINARY_DIR@/$3
cd @CMAKE_BINARY_DIR@/$3
@SPECTRE_TEST_RUNNER@ @CMAKE_BINARY_DIR@/bin/$1 --input-file $2 ${4} &&
    @Python_EXECUTABLE@ @CMAKE_SOURCE_DIR@/tools/CheckOutputFiles.py \
     --input-file $2 --run-directory @CMAKE_BINARY_DIR@/$3 &&
@Python_EXECUTABLE@ @CMAKE_SOURCE_DIR@/tools/CleanOutput.py -v \
                    --input-file $2 --output-dir @CMAKE_BINARY_DIR@/$3
rm -rf @CMAKE_BINARY_DIR@/$3
