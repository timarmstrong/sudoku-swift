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

#include "sudoku_solve.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define BUF_SIZE (BOARD_CELLS * 10)

#ifndef BFS
#define BFS (false)
#endif

int main(int argc, char **argv) {
  init_solver(0);

  fprintf(stderr, "Sudoku solver for %ix%i boards\n", BOARD_WIDTH, BOARD_WIDTH);

  if (argc == 1)
  {
    fprintf(stderr, "No input puzzles provided\n");
    return 0;
  }

  for (int arg = 1; arg < argc; arg++) {
    FILE * in = fopen(argv[arg], "r");
    if (in == NULL) {
      fprintf(stderr, "Could not open input file %s, exiting\n",
              argv[arg]);
      exit(1);
    }

    fprintf(stderr, "Solving puzzles in input file %s\n", argv[arg]);

    char buf[BUF_SIZE];

    while (fgets(buf, BUF_SIZE, in) != NULL) {
      cell_t *sud = board_text_to_bin(buf);
      if (sud == NULL) {
        fprintf(stderr, "Couldn't parse board, skipping\n");
        continue;
      }
      struct board *init = create_board(sud);
      printf("Start board:\n");
      print_board(stdout, init);
      struct boardlist *prog = NULL;
      if (BFS) {
        struct boardlist *candidates;
        candidates = sudoku_solver(init, true, /*1024 * 128*/ 32);
        if (candidates != NULL) {
          if (candidates->len == 1 && 
             candidates->arr[0]->nfilled == BOARD_CELLS) {
            prog = candidates;
          } else {
            for (int i = 0; i < candidates->len; i++) {
              prog = sudoku_solver(candidates->arr[i], false, -1);
              candidates->arr[i] = NULL;
              if (prog != NULL) {
                // found a solution
                free_boardlist(candidates);
                break;
              }
            }
          }
        }
      } else {
        prog = sudoku_solver(init, false, -1);
      }

      if (prog == NULL) {
        fprintf(stderr, "could not solve!\n");
        printf("unsolved:%s\n", buf);
      } else {
        assert(prog->arr[prog->len - 1]->nfilled == BOARD_CELLS);
        printf("Solved!\n");
        print_board(stdout, prog->arr[prog->len - 1]);
        free_boardlist(prog);
      }
      free(sud);
    }
    fclose(in);
  }
}
