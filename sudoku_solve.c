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

#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>

#ifdef TRACE
#define DPRINTF(...) fprintf(stderr,  __VA_ARGS__)
#define DPRINT_BOARD(...) print_board(__VA_ARGS__)
#define DDUMP_MASK(m) dump_mask(m)
#else
#define DPRINTF(...)
#define DPRINT_BOARD(...)
#define DDUMP_MASK(m)
#endif
/******************************************************************************
 * Constants and constant data structures
 ******************************************************************************/
#define BUF_SIZE (BOARD_CELLS * 10)
static mask_t num_masks[N_VALUES];
bool solver_init = false;

/******************************************************************************
 * Solver data structures
 ******************************************************************************/
#define MASK_MASK ((((uint64_t)1) << (N_VALUES % MASK_ELEM_BITS)) - 1)

struct cell {
    int row;
    int col;
};

struct changestack {
    struct cell *arr;
    int len;
    int size;
};

/******************************************************************************
 * Data structure helper functions
 ******************************************************************************/
static inline struct board *clone_board(struct board *board);

static inline void mask_or(mask_t *mask1, mask_t mask2);
static inline void mask_not(mask_t *mask);
static inline void mask_and(mask_t *mask1, mask_t mask2);
static inline mask_t get_mask(struct board *b, int row, int col);
static inline void dump_mask(mask_t mask);
static inline int mask_popcount(mask_t mask);

#define get_cell(board, row, col) (board[row * BOARD_WIDTH + col])
static inline void set_cell(struct board *b, int row, int col, int val);
static inline int get_block(int row, int col);
static inline int block_start_col(int block);
static inline void change_push(struct changestack *stack, int row, int col);
static inline struct cell change_pop(struct changestack *stack);

static inline void add_board(struct boardlist *list, struct board *board);
static inline struct board *remove_last_board(struct boardlist *list);
static inline void bump_boards(struct boardlist *list, int bump);
static void init_boardlist(struct boardlist *list, int init_size);
static inline void boardlist_resize(struct boardlist *boards, int neededsize);

/******************************************************************************
 * Core solver algorithm
 ******************************************************************************/
static void solve_step(struct board *start, struct boardlist *boards);
static bool check_cell(struct board *b, int row, int col, struct changestack *stack, bool firstpass);
static void trace_effects(struct board *b, int row, int col, mask_t changemask,
           struct changestack *stack);
static struct cell best_branchpoint(struct board *b);
static void do_branches(struct board *start, int row, int col, mask_t mask,
        struct boardlist *boards);


void init_solver(unsigned seed) {
    if (!solver_init) {
        srand(seed);
        for (int i = 0; i < N_VALUES; i++) {
            memset(&(num_masks[i]), 0, sizeof(mask_t));
            int off = i / MASK_ELEM_BITS;
            assert(off < MASK_SIZE);
            num_masks[i].vec[off] = ((uint64_t)1) << (i % MASK_ELEM_BITS);
            //DPRINTF("mask %i: ", i); DDUMP_MASK(num_masks[i]);
        }
        solver_init = true;
    }
}

static inline void change_push(struct changestack *stack, int row, int col) {
    if (stack->size <= stack->len) {
        stack->size *= 2;
        stack->arr = realloc(stack->arr, stack->size * sizeof(struct cell));
        assert(stack->arr != NULL);
    }
    assert(row >= 0 && row < BOARD_WIDTH);
    assert(col >= 0 && col < BOARD_WIDTH);
    stack->arr[stack->len].row = row;
    stack->arr[stack->len].col = col;
    stack->len++;
}

static inline struct cell change_pop (struct changestack *stack) {
    struct cell c = stack->arr[stack->len - 1];
    if (!((c.row >= 0 && c.row <= BOARD_WIDTH)
        && (c.col >= 0 && c.col <= BOARD_WIDTH))) {
        fprintf(stderr, "bad [%d][%d]\n", c.row, c.col);
        assert(false);
    }
    stack->len--;
    return c;
}

static inline int get_block(int row, int col) {
    int blockRow = row / BLOCK_WIDTH;
    int blockCol = col / BLOCK_WIDTH;
    return blockRow * BLOCK_WIDTH + blockCol;
}

static inline int block_start_col(int block) {
    return ( (block % BLOCK_WIDTH) * BLOCK_WIDTH);
}

static inline int block_start_row(int block) {
    return ( (block / BLOCK_WIDTH) * BLOCK_WIDTH);
}

static inline void set_cell(struct board *b, int row, int col, int val) {
    if (get_cell(b->board, row, col) != 0) {
        DPRINTF("was %d\n", (int)get_cell(b->board, row, col));
    }
    assert(get_cell(b->board, row, col) == 0);
    mask_t valmask = num_masks[val-1];
#ifndef NDEBUG
    mask_t tmp;
    tmp = valmask;
    mask_and(&tmp, b->col_masks[col]);
    assert(mask_popcount(tmp) == 0);
    tmp = valmask;
    mask_and(&tmp, b->row_masks[row]);
    assert(mask_popcount(tmp) == 0);
    tmp = valmask;
    mask_and(&tmp, b->block_masks[get_block(row, col)]);
    assert(mask_popcount(tmp) == 0);
#endif
    get_cell(b->board, row, col) = val;

    mask_or(&(b->col_masks[col]), valmask);
    mask_or(&(b->row_masks[row]), valmask);
    int block = get_block(row, col);
    mask_or(&(b->block_masks[block]), valmask);
    b->nfilled++;
}

static inline void mask_or(mask_t *mask1, mask_t mask2) {
    for (int i = 0; i < MASK_SIZE; i++) {
        mask1->vec[i] |= mask2.vec[i];
    }
}


static inline void dump_mask(mask_t mask) {
    for (int j = MASK_SIZE - 1; j >= 0; j--) {

#if __WORDSIZE == 32
#warning "1"
      fprintf(stderr, "%016llx ", mask.vec[j]);
#elif __WORDSIZE == 64
#warning "2"
      fprintf(stderr, "%016lx ", mask.vec[j]);
#else
#error "Weird integer sizes"
#endif

    }
    fprintf(stderr, "\n");
}

static inline void mask_and(mask_t *mask1, mask_t mask2) {
    for (int i = 0; i < MASK_SIZE; i++) {
        mask1->vec[i] &= mask2.vec[i];
    }
}

static inline void mask_not(mask_t *mask) {
    for (int i = 0; i < MASK_SIZE; i++) {
        mask->vec[i] = ~(mask->vec[i]);
    }
    // Set bits not in mask to zero
    mask->vec[MASK_SIZE - 1] &= MASK_MASK;
}

static inline mask_t get_mask(struct board *b, int row, int col) {
    mask_t mask;
    memset(&mask, 0, sizeof(mask_t));
    mask_or(&mask, b->col_masks[col]);
    mask_or(&mask, b->row_masks[row]);
    mask_or(&mask, b->block_masks[get_block(row, col)]);
    // Mask now have 1 for each possible position
    mask_not(&mask);
    //DPRINTF("[%d][%d]", row, col); DDUMP_MASK(mask);
    return mask;
}

cell_t *read_sudoku_file(char *file) {
    FILE * in = fopen(file, "r");
    if (in == NULL) {
        fprintf(stderr, "could not open file %s\n", file);
        return NULL;
    }
    char buf[10 * BOARD_CELLS];
    if (fgets(buf, BUF_SIZE, in) == NULL) {
        fprintf(stderr, "could not read line from file %s\n", file);
        return NULL;
    }
    cell_t *sud = board_text_to_bin(buf);
    if (sud == NULL) {
        // error message already printed
        return NULL;
    }
    return sud;
}
size_t cells_mem() {
  return CELLS_MEM; 
}

char *board_bin_to_text(cell_t *src) {
  int size = 3 * BOARD_CELLS + 1;
  char *out = malloc(sizeof(char) * size);
  int pos = 0;
  for (int i = 0; i < BOARD_CELLS; i++) {
    cell_t cl = src[i];
    int nch;
    if (cl == '\0') {
        nch = 2; // '. '
    } else {
        // Check buffer size
        nch = snprintf(NULL, 0, "%d ", cl);
    }

    if (nch + pos >= size - 1) {
        size = size + BOARD_CELLS;
        out = realloc(out, size);
        assert(out != NULL);
    }
    if (cl == '\0') {
        out[pos++] = '.';
        out[pos++] = ' ';
    } else {
        pos += sprintf(out + pos, "%d ", cl);
    }
  }
  // Replace last space with null terminator
  out[pos - 1] = '\0';
  return out;
}

cell_t *board_text_to_bin(char *src) {
    cell_t *dst = malloc(sizeof(cell_t) * BOARD_CELLS);
    int pos = 0;
    int nread = 0;
    while (src[pos] != '\0') {
        while(isspace(src[pos])) {
            pos++;
        }
        if (src[pos] == '\0') {
            break;
        }
        if (nread >= BOARD_CELLS) {
            free(dst);
            fprintf(stderr, "wrong number of cells: %d last cell %c\n", nread, src[pos]);
            return NULL;
        }
        if (src[pos] == '.') {
            dst[nread++] = 0;
            pos++;
        } else if (isdigit(src[pos])) {
            int val = atoi(src + pos);
            if (val < 0 || val > N_VALUES) {
                fprintf(stderr, "invalid value %d at cell %d\n", val, pos);
                return false;
            }
            while(isdigit(src[pos])) {
                pos++;
            }
            dst[nread++] = val;
        } else {
            free(dst);
            fprintf(stderr, "invalid character %c in string\n", src[pos]);
            return NULL;
        }
    }
    if (nread < BOARD_CELLS) {
        free(dst);
        fprintf(stderr, "not enough cells: %d/%d \n", nread, BOARD_CELLS);
        return NULL;
    }
    return dst;
}

void free_cells(cell_t *cells) {
    free(cells);
}

uint64_t ptr_convert(cell_t *cells) {
  return (uint64_t) cells;
}
cell_t *cells_convert(uint64_t ptr) {
  return (cell_t *) ptr;
}

struct board *create_board(cell_t *init_board) {
    struct board *b = (struct board*)malloc(sizeof(struct board));
    assert(b != NULL);

    memcpy(b->board, init_board, BOARD_CELLS * sizeof(cell_t));

    memset(b->col_masks, 0, sizeof(b->col_masks));
    memset(b->row_masks, 0, sizeof(b->row_masks));
    memset(b->block_masks, 0, sizeof(b->block_masks));

    int filled = 0;
    for (int row = 0; row < BOARD_WIDTH; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            int val = get_cell(b->board, row, col);
            if (val != 0) {
                mask_t mask = num_masks[val-1];
                mask_or(&(b->col_masks[col]), mask);
                mask_or(&(b->row_masks[row]), mask);
                int block = get_block(row, col);
                mask_or(&(b->block_masks[block]), mask);
                filled++;
            }
        }
    }
    b->nfilled = filled;
    return b;
}

static inline struct board *clone_board(struct board *board) {
    struct board *newboard = (struct board*)malloc(sizeof(struct board));
    if (newboard == NULL) {
        fprintf(stderr, "Ran out of memory in clone_board\n");
        exit(1);
    }
    memcpy(newboard, board, sizeof(struct board));
    return newboard;
}

static inline void boardlist_resize(struct boardlist *boards, int neededsize) {
    // resize
    if (boards->size < neededsize) {
        boards->size *= 2;
        boards->arr=realloc(boards->arr,
                        boards->size * sizeof(struct board*));
        assert(boards->arr != NULL);
    }
}

void init_boardlist(struct boardlist *list, int init_size) {
    list->arr = (struct board**)malloc(sizeof(struct board*) * init_size);
    assert(list->arr != NULL);
    list->size = init_size;
    list->len = 0;
}

static inline void add_board(struct boardlist *list, struct board *board) {
    boardlist_resize(list, list->len + 1);
    list->arr[list->len] = board;
    list->len++;
}

static inline struct board *remove_last_board(struct boardlist *list) {
    assert(list->len > 0);
    struct board *result = list->arr[list->len - 1];
#ifndef NDEBUG
    list->arr[list->len - 1] = NULL;
#endif
    list->len--;
    return result;
}

// NOTE: doesn't free bumped boards
static inline void bump_boards(struct boardlist *list, int bump) {
    assert(bump <= list->len);
    if (bump == 0) {
        return;
    }
    // Remove the processed boards
    for (int i = bump; i < list->len; i++) {
        list->arr[i-bump] = list->arr[i];
#ifndef NDEBUG
        list->arr[i] = NULL;
#endif
    }
    list->len -= bump;
}

void free_board(struct board *board) {
    free(board);
}

void free_boardlist(struct boardlist *l) {
    for (int i = 0; i < l->len; i++) {
       if (l->arr[i] != NULL) {
           free_board(l->arr[i]);
       }
    }
    free(l->arr);
    free(l);
}


int board_nfilled(struct board *b) {
    return b->nfilled;
}

int boardlist_len(struct boardlist *l) {
    return l->len;
}

struct board *boardlist_get(struct boardlist *l, int i) {
    assert(i < l->len);
    return l->arr[i];
}

void print_rowsep(FILE *out) {
    for (int col = 0; col < BOARD_WIDTH; col++) {
        if ((col % BLOCK_WIDTH) == 0) {
            fprintf(out, "+-");
        }
        fprintf(out, "---");
    }
    fprintf(out, "+\n");
}

void print_board(FILE *out, struct board *b) {
    for (int row = 0; row < BOARD_WIDTH; row++) {
        if ((row % BLOCK_WIDTH) == 0) {
            print_rowsep(out);
        }
        for (int col = 0; col < BOARD_WIDTH; col++) {
            if ((col % BLOCK_WIDTH) == 0)
                fprintf(out, "| ");
            fprintf(out, "%2d ", get_cell(b->board, row, col));
        }
        fprintf(out, "|\n");
    }
    print_rowsep(out);
}

void print_board_stdout(struct board *b) {
    print_board(stdout, b);
}

void print_board_stderr(struct board *b) {
    print_board(stderr, b);
}

struct boardlist *sudoku_solver(struct board *start, bool breadthfirst,
                                                            long quota) {
    assert(solver_init);
    //fprintf(stderr, "sudoku_solver enter\n");
    // Init number masks
    // Init start board

    struct boardlist *boards = malloc(sizeof(struct boardlist));
    assert(boards != NULL);
    init_boardlist(boards, 2 * (breadthfirst && quota > 1024 ? quota: 1024));
    add_board(boards, start);

    return sudoku_solver_resume(boards, breadthfirst, quota);
}

struct boardlist *sudoku_solver_resume(struct boardlist *boards, bool breadthfirst,
                                                            long quota) {
    // Keep exploring until either we have generated enough candidates or
    // we've exhausted all branches
    bool solved = false;
    long pass = 0;
    if (breadthfirst) {
        while (!solved && (quota < 0 || boards->len < quota) && boards->len > 0) {
#ifndef NDEBUG
            fprintf(stderr, "sudoku_solver start BFS pass %ld: "
                    "%d candidate boards\n", pass, boards->len);
#endif
            int n = boards->len;
            int toremove = n;
            int i;
            for (i = 0; i < n; i++) {
                struct board *curr = boards->arr[i];
                DPRINTF("candidate %d/%d %d\n", i+1, n, curr->nfilled);
#ifndef NDEBUG
                boards->arr[i] = NULL;
#endif
                int oldlen = boards->len;
                // append new boards to end of array
                solve_step(curr, boards);

                solved = boards->len - oldlen == 1 &&
                        boards->arr[boards->len - 1]->nfilled == BOARD_CELLS;

                if (solved) {
                    // remove all but solution
#ifndef NDEBUG
                    fprintf(stderr, "SOLVED!\n");
#endif
                    toremove = boards->len - 1;
                    break;
                }
                if ((quota >= 0 && (boards->len - i - 1) > quota)) {
                    toremove = i + 1;
                    break;
                }
            }
            DPRINTF("iterated over %d candidates, now %d total\n",
                        toremove, boards->len);
            bump_boards(boards, toremove);
            DPRINTF("After bump: %d total\n",
                        boards->len);
            pass++;
        }
    } else {
        while ((quota < 0 || boards->len <= 1 ||
                    pass < quota + ((boards->len / 2) * quota))
                            && boards->len > 0 && !solved) {
    #ifndef NDEBUG
            fprintf(stderr, "sudoku_solver start pass %ld: %d candidate boards\n", pass, boards->len);
    #endif
            struct board *curr = remove_last_board(boards);
            // DFS
            int oldlen = boards->len;
            solve_step(curr, boards);


            int newboards = boards->len - oldlen;
            assert(newboards >= 0 && newboards <= N_VALUES);
            if (newboards == 1 && boards->arr[boards->len-1]->nfilled == BOARD_CELLS) {
               solved = true;
               for (int i=0; i < boards->len - 1; i++) {
                 free(boards->arr[i]);
#ifndef NDEBUG
                 boards->arr[i] = NULL;
#endif
               }
               assert(boards->arr[boards->len-1] != NULL);
               assert(boards->arr[boards->len-1]->nfilled == BOARD_CELLS);
               bump_boards(boards, boards->len - 1);
               assert(boards->len == 1);
               assert(boards->arr[0] != NULL);
               assert(boards->arr[0]->nfilled == BOARD_CELLS);
               assert(boardlist_solved(boards));
            }
            pass++;
        }
    }

    if (boards->len == 0) {
        free_boardlist(boards);
        return NULL;
    } else {
        return boards;
    }
}

bool boardlist_solved(struct boardlist *boards) {
    if (boards != NULL) {
        if (boards->len == 1) {
            if (boards->arr[0]->nfilled == BOARD_CELLS) {
                return true;
            }
        }
    }
    return false;
}


/*
 * Look at board start, see which cells can be filled in, and then branch at
 * a cell chosen by a heuristic.  The branches are added to the end of boardlist.
 *
 * Dynamically resizes boards if needed
 * This should either copy the pointer to start into the array at a later position, or
 * free start
 */
void solve_step(struct board *start, struct boardlist *boards) {
    DPRINTF("Enter solve_step\n");
    DPRINT_BOARD(stderr, start);
    assert(start != NULL);
    assert(boards != NULL);
    struct changestack stack;
    stack.size = 1024;
    stack.len = 0;
    stack.arr = malloc(sizeof(struct cell) * stack.size);

    // make initial pass over array until all directly constrained cells
    //  are filled in (those we can determine just by looking at row, column
    //  and block)
    //  Then branch on the most constrained choice (least number of possibilities)
    for (int row = 0; row < BOARD_WIDTH; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            DPRINTF("Solve_step: first pass cell[%d][%d]\n", row, col);
            bool ok = check_cell(start, row, col, &stack, true);
            if (!ok) {
                // no viable solution
                DPRINTF("Not viable\n");
                free_board(start);
                free(stack.arr);
                return;
            }
        }
    }

    DPRINTF("Solve_step: first pass done, %d items in stack \n", stack.len);

    // Propagate constraints and see if we can fill out more cells
    while (stack.len > 0) {
        struct cell c = change_pop(&stack);
        bool ok = check_cell(start, c.row, c.col, &stack, false);
        if (!ok) {
            // no viable solution
            DPRINTF("Not viable\n");
            free_board(start);
            free(stack.arr);
            return;
        }
    }

    DPRINTF("Solve_step done propagating constraints, %d filled\n", start->nfilled);
    DPRINT_BOARD(stderr, start);

    if (start->nfilled == BOARD_CELLS) {
        // Solved!
        // put solution in last spot of array
        add_board(boards, start);
        DPRINTF("FOUND SOLUTION\n");
    } else {
        struct cell bp = best_branchpoint(start);
        DDUMP_MASK(get_mask(start, bp.row, bp.col));
        do_branches(start, bp.row, bp.col,
                    get_mask(start, bp.row, bp.col),
                    boards);
    }
    free(stack.arr);
}

static struct cell best_branchpoint(struct board *b) {
    int bestrow = -1;
    int bestcol = -1;
    int minbranches = N_VALUES + 1;
    int equalbestcount = 0;

    for (int row = 0; row < BOARD_WIDTH; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            if (get_cell(b->board, row, col) == 0) {
                mask_t mask = get_mask(b, row, col);
                int nchoices = mask_popcount(mask);
                if (nchoices < minbranches) {
                    bestrow = row;
                    bestcol = col;
                    minbranches = nchoices;
                    equalbestcount = 1;
                }
#ifdef RANDOM_BRANCH
                else if (nchoices == minbranches) {
                    equalbestcount++;
                    // Randomize selection among equals
                    // Choose this one with p=1/k, where k is number of
                    // alternatives found so far.  This guarantees each poss
                    // selected with equal probability
                    int r = rand() % equalbestcount;
                    if (r == 0) {
                        bestrow = row;
                        bestcol = col;
                    }
                }
#endif
            }
        }
    }
    assert(bestrow >= 0 && bestcol >= 0);
    assert(minbranches <= N_VALUES);
    struct cell res;
    res.row = bestrow;
    res.col = bestcol;
    return res;
}

bool check_cell(struct board *b, int row, int col,
            struct changestack *stack, bool firstpass) {
    assert(b != NULL);
    if (get_cell(b->board, row, col) == 0) {
        mask_t mask = get_mask(b, row, col);
        int nchoices = mask_popcount(mask);
        //DPRINTF(stderr, "[%d, %d] mask %x choices %d \n", row, col, mask, nchoices);
        assert(nchoices <= N_VALUES);
        if (nchoices == 0) {
            // No viable solution
            DPRINTF("Backtracking: [%d][%d]\n", row, col);
            DPRINT_BOARD(stderr, b);
            DPRINTF("  row mask:   "); DDUMP_MASK(b->row_masks[row]);
            DPRINTF("  col mask:   ");DDUMP_MASK(b->col_masks[col]);
            DPRINTF("  block mask: ");DDUMP_MASK(b->block_masks[get_block(row,col)]);
            return false;
        } else if (nchoices == 1) {
            DPRINTF("constrained [%d][%d]\n", row, col);
            // is power of two => unique value
            int sol = 1;
            int pos = 0;
            while (mask.vec[pos] == 0) {
                pos++;
                sol += MASK_ELEM_BITS;
            }
            uint64_t maskelem = mask.vec[pos];

            while (maskelem != 1) {
                maskelem >>= 1;
                sol++;
            }
            mask_t changemask = num_masks[sol-1];

            // Don't need to look forward on first pass
            int maxcol = firstpass ? col : BOARD_WIDTH;
            for (int col2 = 0; col2 < maxcol; col2++) {
                if (col2 != col) {
                   trace_effects(b, row, col2, changemask, stack);
                }
            }

            int maxrow = firstpass ? row : BOARD_WIDTH;
            for (int row2 = 0; row2 < maxrow; row2++) {
                if (row2 != row) {
                   trace_effects(b, row2, col, changemask, stack);
                }
            }

            int block = get_block(row, col);
            int startcol = block_start_col(block);
            int startrow = block_start_row(block);
            maxrow = firstpass ? row + 1: startrow + BLOCK_WIDTH;
            for (int row2 = startrow; row2 < maxrow; row2++) {
                for (int col2 = startcol; col2 < startcol + BLOCK_WIDTH; col2++) {
                    if (row2 != row || col2 != col) {
                        if (!(firstpass && (row2 > row || (row2 == row && col2 > col)))) {
                            trace_effects(b, row2, col2, changemask, stack);
                        }
                    }
                }
            }
            set_cell(b, row, col, sol);
        }
    }
    return true;
}


void do_branches(struct board *start, int row, int col, mask_t mask,
            struct boardlist *boards) {
#ifndef NDEBUG
    fprintf(stderr, "BRANCHING [%d][%d]:\n", row, col);
#endif
    int top = -1;
    for (int i = 0; i < MASK_SIZE; i++) {
        if (mask.vec[i] != 0) {
            top = i;
        }
    }
    assert(top >= 0);
    for(int i = 0; i <= top; i++) {
        uint64_t maskp = mask.vec[i];
        int cellchoice = MASK_ELEM_BITS * i + 1;
        while (maskp != 0) {
            if ((maskp & 0x1) != 0) {
                struct board *newboard = NULL;
                if (maskp == 1 && i == top) {
                    // LAST
                    newboard = start;
                    start = NULL;
                } else {
                    // copy board & masks
                    newboard = clone_board(start);
                }
#ifndef NDEBUG
                fprintf(stderr, "choice: %d\n", cellchoice);
#endif
                DPRINTF("branch: ");
                set_cell(newboard, row, col, cellchoice);
                add_board(boards, newboard);
            }
            cellchoice++;
            maskp = maskp >> 1;
        }
    }
}

void trace_effects(struct board *b, int row, int col, mask_t changemask,
           struct changestack *stack) {
    int block = get_block(row, col);
    mask_t oldmask = b->row_masks[row];
    mask_or(&oldmask, b->col_masks[col]);
    mask_or(&oldmask, b->block_masks[block]);
    for (int i = 0; i < MASK_SIZE; i++) {
        uint64_t overlap = oldmask.vec[i] & changemask.vec[i];
        if (overlap != 0) {
            return;
        }
    }
    // no overlap - constraints on [row, col] changed
    change_push(stack, row, col);
}

static inline int mask_popcount(mask_t mask) {
    int res = 0;
    for (int i = 0; i < MASK_SIZE; i++) {
        res += __builtin_popcountll(mask.vec[i]);
    }
//    fprintf(stderr, "popcount %d ", res); DDUMP_MASK(mask);
    return res;
}
