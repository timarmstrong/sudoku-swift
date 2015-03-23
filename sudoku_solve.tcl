namespace eval sudoku {

    variable mpe_ready

    variable event

    proc mpe_setup { } {

        variable mpe_ready
        variable event

        if [ info exists mpe_ready ] return
        set mpe_ready 1

        set L [ mpe::create_pair sudoku ]
        set event(start_sudoku) [ lindex $L 0 ]
        set event(end_sudoku)   [ lindex $L 1 ]
    }

    proc parse_board { fname } {
        set board_ptr [ ptr_convert [ read_sudoku_file $fname ] ]
        return [ list $board_ptr [ cells_mem ] ]
    }

    proc sudoku_step { output solved board breadthfirst quota } {
        variable event

        mpe_setup
        mpe::log $event(start_sudoku)

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

        if { $boardl != {NULL} } {
            set n [ boardlist_len_get $boardl ]
            # puts stderr "${n} new boards at [ clock clicks -milliseconds ]"
            if { $n > 0 } {
              set tds [ adlb::multicreate {*}[ lrepeat $n [ list blob 1 ] \
                                                       [ list integer 1 ] ] ]
            } else {
              set tds [ list ]
            }

            set output_contents [ dict create ]

            for { set i 0 } { $i < $n } { incr i } {
                set board [ boardlist_get $boardl $i ]
                set board_cells [ board_board_get $board ]
                set filled [ board_nfilled $board ]
                #puts stderr "Next board: ${board_text} filled ${filled}"

                # Set the blob
                set board_td [ lindex $tds [ expr 2 * $i ] ]
                set filled_td [ lindex $tds [ expr 2 * $i + 1 ] ]
                turbine::store_blob $board_td [ list [ ptr_convert $board_cells ] [ cells_mem ] ]
                turbine::store_integer $filled_td $filled

                set struct [ dict create "board" $board_td "filledSquares" $filled_td ]
                dict append output_contents $i $struct
            }

            # TODO: better struct type naming
             turbine::array_kv_build $output $output_contents 1 integer struct0

            # free board list
            free_boardlist $boardl
        } else {
            # puts stderr "Dead end at [ clock clicks -milliseconds ]!"
            # Won't write
            adlb::write_refcount_decr $output
        }

        turbine::log "sudoku_step_body done => $output"
        mpe::log $event(end_sudoku)
    }

    proc print_board_tcl { board } {
        set board_internal [ create_board [ cells_convert [ lindex $board 0 ] ] ]
        print_board_stderr $board_internal
    }
}
