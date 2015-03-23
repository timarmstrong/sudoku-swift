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

#ifndef __SUDOKU_SOLVE_H
#define __SUDOKU_SOLVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifndef BLOCK_WIDTH
#define BLOCK_WIDTH (3)
#endif 

#define BOARD_WIDTH (BLOCK_WIDTH * BLOCK_WIDTH)
#define BOARD_CELLS (BOARD_WIDTH * BOARD_WIDTH)
#define CELLS_MEM (sizeof(cell_t) * BOARD_CELLS)
#define N_VALUES BOARD_WIDTH
#define MASK_ELEM_BITS (64)
#define MASK_SIZE ( ((N_VALUES - 1) / MASK_ELEM_BITS) + 1)

struct mask {
    uint64_t vec[MASK_SIZE];
};

typedef struct mask mask_t;
typedef unsigned char cell_t;

/* each row is stored contiguously
 * board[row *BOARD SIZE + col]
 * char 0x0 -> nothing entered, char 0x1 -> number one in cell, etc */
struct board {
    cell_t board[BOARD_CELLS];
    int nfilled;
    // Track which are possible numbers for each cell
    // Masks have a bit set for each number that is already used
    mask_t col_masks[BOARD_WIDTH];
    mask_t row_masks[BOARD_WIDTH];
    mask_t block_masks[BOARD_WIDTH];
};

struct boardlist {
    struct board **arr;
    int size;
    int len;
};


/******************************************************************************
 * Public functions
 ******************************************************************************/
void init_solver(unsigned seed);
// Parse text description with '.' meaning 0
cell_t *board_text_to_bin(char *src);
char *board_bin_to_text(cell_t *src);
void free_cells(cell_t *cells);
uint64_t ptr_convert(cell_t *cells);
cell_t *cells_convert(uint64_t ptr);

cell_t *read_sudoku_file(char *file);
size_t cells_mem();
struct board *create_board(cell_t *init_board);
void free_board(struct board *board);
void print_board(FILE *out, struct board *b);
void print_board_stdout(struct board *b);
void print_board_stderr(struct board *b);
struct boardlist *sudoku_solver(struct board *start, bool breadthfirst,
                                                            long quota);

struct boardlist *sudoku_solver_resume(struct boardlist *boards, bool breadthfirst,
                                                            long quota);
bool boardlist_solved(struct boardlist *boards);

struct board *boardlist_get(struct boardlist *l, int i);
int boardlist_len(struct boardlist *l);
int board_nfilled(struct board *b);
void free_boardlist(struct boardlist *l, bool free_boards);

#endif //__SUDOKU_SOLVE_H
