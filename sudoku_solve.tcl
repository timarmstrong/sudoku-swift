namespace eval sudoku {

    proc parse_board { fname } {
        set board_ptr [ ptr_convert [ read_sudoku_file $fname ] ]
        return [ list $board_ptr [ cells_mem ] ]
    }

    proc sudoku_step { solved board breadthfirst quota } {

        #turbine::log "sudoku_step_body $board $breadthfirst $quota => $output"

        # use standard seed for now
        init_solver 0

        # puts stderr "sudoku_step_body <${board}>"


        if { $solved > 0.0 } {
            # done: close output and exit
            turbine::log "solved elsewhere!"
            adlb::write_refcount_decr $output
            return
        }

        #puts stderr "Retrieved board string ${board_str}"
        #puts stderr "breadthfirst: ${breadthfirst_val} quota: ${quota_val}"

        set board_internal [ create_board [ cells_convert [ lindex $board 0 ] ] ]

        set boardl [ sudoku_solver $board_internal $breadthfirst $quota ]
        # board_internal ref taken over by solver
        unset board_internal

        set result [ dict create ]

        if { $boardl != {NULL} } {
            set n [ boardlist_len_get $boardl ]
            # puts stderr "${n} new boards at [ clock clicks -milliseconds ]"

            for { set i 0 } { $i < $n } { incr i } {
                set board [ boardlist_get $boardl $i ]
                set board_cells [ board_board_get $board ]
                set filled [ board_nfilled $board ]
                #puts stderr "Next board: ${board_text} filled ${filled}"

                # Set the blob
                set board [ list [ ptr_convert $board_cells ] [ cells_mem ] ]

                set struct [ dict create "board" $board "filledSquares" $filled ]
                dict append result $i $struct
            }
            
            # Free list but not boards
            free_boardlist $boardl 0

        } else {
            # puts stderr "Dead end at [ clock clicks -milliseconds ]!"
            # Won't write
        }

        turbine::log "sudoku_step_body done => $result"
        return $result
    }

    proc print_board_tcl { board } {
        set board_internal [ create_board [ cells_convert [ lindex $board 0 ] ] ]
        print_board_stderr $board_internal
    }
}
