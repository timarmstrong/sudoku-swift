#!/bin/bash
set -x

SCRIPTDIR=$(dirname $0)

if [ -z "$TURBINE_HOME" ]; then
  TURBINE=$(which turbine)
  if [ ! -x "$TURBINE" ]; then
    echo "turbine not on path"
    exit 1
  fi

  TURBINE_HOME=$(cd "$(dirname $TURBINE)/.."; pwd)
fi
if [ ! -d "$TURBINE_HOME" ]; then
  echo "Using Turbine install not found at ${TURBINE_HOME}"
fi

echo "Using Turbine install: ${TURBINE_HOME}"

MKSTATIC="${TURBINE_HOME}/scripts/mkstatic/mkstatic.tcl"
if [ ! -x "$MKSTATIC" ]; then
  echo "Could not find mkstatic.tcl in Turbine install at $MKSTATIC"
  exit 1
fi

# Build the Sudoku leaf package

# Set CC if desired

# This could be a Makefile but I think it is better
# to use bash as a reference example. -Justin

LEAF_PKG="sudoku_solve"
LEAF_I=${LEAF_PKG}.i
LEAF_C=${LEAF_PKG}_wrap.c
LEAF_O=${LEAF_C%.c}.o
LEAF_SO="lib${LEAF_PKG}.so"
LEAF_TCL="${LEAF_PKG}.tcl"

LEAF_MAIN="sudoku_swift"
LEAF_MAIN_C="${LEAF_MAIN}.c"
LEAF_MANIFEST="${LEAF_MAIN}.manifest"
LEAF_VERSION="0.0"

export CC=${CC:-mpicc}

check()
{
  CODE=${?}
  if [[ ${CODE} != 0 ]]
  then
    MSG=$1
    echo ${MSG}
    exit ${CODE}
  fi
  return 0
}

source $SCRIPTDIR/build-standalone.sh

# Get Turbine build vars
TURBINE_BUILD_CONFIG=${TURBINE_HOME}/scripts/turbine-build-config.sh
[[ -f ${TURBINE_BUILD_CONFIG} ]]
check "Could not read ${TURBINE_BUILD_CONFIG}!"
source ${TURBINE_BUILD_CONFIG}

echo "using Tcl in ${TCL_HOME}"
echo "Tcl version ${TCL_VERSION}"

#set -x

# Create the Tcl extension
swig -tcl -module ${LEAF_PKG} ${LEAF_I}
check

# Compile the Tcl Extension
${CC} -std=c99 -Wall -g ${TCL_INCLUDE_SPEC} -c ${LEAF_C}
check

echo "created package."

# Compile the Swift code to tcl
stc -L sudoku.stc.log -C ${SCRIPTDIR}/sudoku.{ic,swift,tic}
check
echo "compiled swift source."

# Create main Turbine program and package index, using compiled Tcl code
MKSTATIC_FLAGS="--include-sys-lib ${TCL_SYSLIB_DIR} --tcl-version ${TCL_VERSION}"
${MKSTATIC} ${LEAF_MANIFEST} ${MKSTATIC_FLAGS} -c ${LEAF_MAIN_C}
check

echo TURBINE_INCLUDES=${TURBINE_INCLUDES}
echo TURBINE_LIBS=${TURBINE_LIBS}

# Compile the main program
OBJS="${LEAF_O} ${USER_O}"
${CC} -std=c99 -Wall -g ${LEAF_MAIN_C} \
      ${OBJS} \
      -I. ${TURBINE_INCLUDES} ${TURBINE_LIBS} \
      -o ${LEAF_MAIN}
check

exit 0
