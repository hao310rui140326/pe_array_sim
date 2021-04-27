#!/bin/sh
quartus_sh --flow compile pe_array 2>&1 | tee quartus_sh_compile.log
