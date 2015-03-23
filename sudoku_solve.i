
%module sudoku
%{
#include "sudoku_solve.h"
#include <stdbool.h>
#include <stdint.h>
%}

%include "stdint.i"

// Free returned string
%newobject board_bin_to_text;

%include "sudoku_solve.h"
