A toy Sudoku solver application to illustrate parallelization of a C program
with Swift/T.

Standalone Solver
================
To build the standalone serial solver, run: ./build-standalone.sh

You can set the CC environment variable to your preferred C compiler.

The board size is a compile-time constant.  You can override the default
100x100 size by specifying the BLOCK_WIDTH environment variable at
runtime, e.g. BLOCK_WIDTH=3 for a 9x9 board or BLOCK_WIDTH=10 for a
100x100 board.

You can run the serial solver by providing the puzzles to solve on the
command line:
./sudoku puzzles/100x100easy puzzles/100x100med

Swift/T Parallel Solver
======================
NOTE: this was written against an old version of the Swift/T API.  It
is not working yet.

To build the parallel Swift/T solver, run: ./build-swift.sh.

This assumes that stc, turbine and mpicc are on your path.  

To run the parallel solver, use turbine: turbine -X -x ./sudoku_swift --board=puzzles/100x100 --boardsize=100
