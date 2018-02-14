# Some notes about the xv6 system and qemu simulator

## Execute

* ``make qemu``
Build everything and start QEMU with the VGA console in a new window and the serial console in your terminal. To exit, either close the VGA window or press Ctrl-c or Ctrl-a x in your terminal.

* ``make qemu-nox``
Like make qemu, but run with only the serial console. To exit, press Ctrl-a x. This is particularly useful over SSH connections to Athena dialups because the VGA window consumes a lot of bandwidth.

## Debug
1. In one window, enter ``make qemu-nox-gdb``. Start a gdb version xv6
1. In another window, enter ``gdb``. It will attach a gdb to the qemu process.


## Add user programs
1. In user/, create a .c source file.
1. In user/makefile.mk, add the .c source file to USER\_PROGS.

## Miscs
1. To locate a function, use ``grep -rn "^${FUNCNAME}("``. The xv6 puts the function name at the beginning of the line.
