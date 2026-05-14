open_project nb_hls_proj -reset
set_top nb_hls_top

set CFLAGS "-I../include -I../output -DALLOW_EMPTY_HLS_STREAM_READS"

add_files ../output/nb_hls.cpp           -cflags $CFLAGS
add_files -tb ../tests/hls_tb.cpp        -cflags $CFLAGS

open_solution "solution1"
set_part {xcu26-vsva1365-2LV-e}
create_clock -period 10 -name default

csynth_design
cosim_design -rtl verilog -tool auto
exit
