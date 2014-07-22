#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

$file=join('',<>);
$file =~ s/(.*BEGIN_FUNC\(arm_undefined_inst_exception\)\n).*(END_FUNC\(arm_undefined_inst_exception\)\n.*)/$1    srsia \#PMODE_SUPERVISOR\n    cps \#PMODE_SUPERVISOR\n    blx r8\n    rfeia sp\n$2/s;
print "$file";
