#!/bin/bash
# This scripts performs static code analysis using clang.
BASEDIR=$PWD
BLDDIR=$PWD/bldAnalysis
# cmake needs to find the icc compiler (run bootstrap.sh first)
DISTDIR=$PWD
# http://clang-analyzer.llvm.org/scan-build.html recommends Debug build
BUILD_TYPE=Debug
# executables needed for analysis
# (set full path here if they are not in your default search path)
CCC_ANALYZER=ccc-analyzer
SCAN_BUILD=scan-build

export CC=$CCC_ANALYZER
export CXX=$CCC_ANALYZER

function usage {
    echo "Usage: $0 [-m32] [clean]"
    exit 1
}

# process commandline arguments
while getopts "m:c" opt; do
    case $opt in
        m)
            if [ "$OPTARG" == "32" ]; then
                # build 32bit exeutable on 64bit systems
                BLDDIR="${BLDDIR}32"
                DISTDIR="${DISTDIR}32"
                export CFLAGS="-m32"
            else
                usage
            fi
            ;;
        *)
            usage
            ;;
    esac
done

# shift out arguments processed by getopt
shift $((OPTIND-1))

if [ $# -gt 0 ] && [ "$1" == "clean" ]; then
    rm -rf $BLDDIR
fi

mkdir $BLDDIR
cd $BLDDIR
cmake -DCMAKE_INSTALL_PREFIX=$DISTDIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
$SCAN_BUILD make

