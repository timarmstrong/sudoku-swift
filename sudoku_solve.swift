/* Header file for sudoku leaf functions */

type boardinfo {
  int filledSquares;
  blob board;
}

(blob board) parse_board (string filename) "sudoku_solve" "0.0" [
  "set <<board>> [ sudoku::parse_board <<filename>> ] "
];

@dispatch=WORKER
(boardinfo nextboards[]) sudoku_step(updateable_float solved, blob board, 
                                        boolean breadthfirst, int quota)
                                        "sudoku_solve" "0.0" [
    "set <<nextboards>> [ sudoku::sudoku_step <<solved>> <<board>> <<breadthfirst>> <<quota>> ]"
];

() print_board(blob board) "sudoku_solve" "0.0" [
  "sudoku::print_board_tcl <<board>>"
];
