#!/bin/bash
set -x

SCRIPTDIR=$(dirname $0)

CC=${CC:-cc}

USER_C="sudoku_solve.c"
USER_H=${USER_C%.c}.h
USER_O=${USER_C%.c}.o

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

# Settings to compile the user code
BLOCK_WIDTH=${BLOCK_WIDTH:-10} # the size of each sudoku board block (standard board is 3)
DBG=0
VERBOSE=0

if [[ $( uname -m ) != ppc64 ]]
then
  TUNING="-march=native -mtune=native"
fi
if [ $DBG = 1 ]; then
    if [ $VERBOSE = 1 ]; then
        CC_OPTS="-g "
    else
        CC_OPTS="-g -DNDEBUG"
    fi
else
    CC_OPTS="-O3 $TUNING -DNDEBUG"
fi

# Compile the user code
${CC} -std=c99 -Wall -g ${CC_OPTS} \
    -DBLOCK_WIDTH=$BLOCK_WIDTH -c ${USER_C}
check

# Compile the test program
${CC} -std=c99 -Wall -DBLOCK_WIDTH=$BLOCK_WIDTH ${USER_O} sudoku.c -o sudoku
check
