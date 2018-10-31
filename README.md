# libytpmv
(wip) C++ library of utilities for generating YTPMVs

Library includes:
* .mod parser
* audio renderer
* video renderer

Example programs:
* test1.C: reads a .mod file and renders the original audio to stdout (only rudimentary .mod features supported)
* test2.C: same as test1, but uses the AudioRenderer API
* test3.C: example audio ytpmv; run (after compiling) `./test3 ./test3.mod | aplay -f cd` to hear a ytpmv of <<inside beek's mind>> by edzes

**Example YTPMV videos are at:**

https://github.com/xaxaxa/ytpmv-examples
