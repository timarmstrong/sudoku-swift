/*
 * Copyright 2012-2015 University of Chicago and Argonne National Laboratory
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License
 */

import io;
import sys;
import sudoku_solve;

argv_accept("board", "boardsize", "split1", "split2", "dfsquota");
updateable_float solved = 0;
printf("Opening board file %s", argv("board"));
blob startboard = parse_board(argv("board"));

// first generate a bunch of parallel work
boardinfo candidates1[] = sudoku_step(solved, startboard, true,
                        toint(argv("split1", "32")));
foreach c1 in candidates1 {
    boardinfo candidates2[] = sudoku_step(solved,
        c1.board, true, toint(argv("split2","32")));
    foreach c2 in candidates2 {
        sudoku_solve(solved, c2.board, c2.filledSquares);
    }
}

sudoku_solve (updateable_float solved, blob board, int filled) {
  // Terminate if we've got a solution
  if (solved == 0.0) {
    // number of steps to take before returning
    int quota = toint(argv("dfsquota", "25000"));
    int boardsize = toint(argv("boardsize", "9"));
    boardinfo candidates3[] = @prio=filled sudoku_step(solved, board,
                                                  false, quota);
    foreach c3 in candidates3 {
      int totalSquares = boardsize * boardsize;
      if (c3.filledSquares == totalSquares) {
        solved <incr> := 1;
        print_board(c3.board);
        printf("SOLVED!");
      } else {
        @prio=c3.filledSquares sudoku_solve(solved, c3.board,
                                       c3.filledSquares);
      }
    }
  } else {
    // trace("solved elsewhere, pruning branch");
  }
}
