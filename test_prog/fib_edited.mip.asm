.text
.global main

main: 
    addiu $sp, $sp, -40
    sw    $ra, 36($sp)
    sw    $fp, 32($sp)
    move  $fp, $sp
    li    $v0, 10 
    sw    $v0, 24($fp)
    lw    $a0, 24($fp)
    la    $t9, fib
    jalr  $t9
    nop 
    sw    $v0, 28($fp)
    move  $sp, $fp
    lw    $ra, 36($sp)
    lw    $fp, 32($sp)
    addiu $sp, $sp, 40
    jr    $ra
    nop 

fib: 
    addiu $sp, $sp, -48
    sw    $ra, 44($sp)
    sw    $fp, 40($sp)
    sw    $s0, 36($sp)
    move  $s8, $sp
    sw    $a0, 48($fp)
    lw    $v0, 48($fp)
    nop
    slti  $v0, $v0, 3 
    beqz  $v0, fib_recursive
    nop 
    li    $v0, 1
    j     fib_prolog
    nop 
fib_recursive:
    lw    $v0, 48($fp)
    nop 
    addiu $v0, $v0, -1 
    move  $a0, $v0
    la    $t9, fib
    jalr  $t9
    nop 
    move  $s0, $v0
    lw    $v0, 48($fp)
    nop
    addiu $v0, $v0, -2
    move  $a0, $v0 
    la    $t9, fib
    jalr  $t9 
    nop 
    addu  $v0, $s0, $v0
    sw    $v0, 24($fp)
    lw    $v0, 24($fp)
fib_prolog:
    move  $sp, $fp
    lw    $ra, 44($fp)
    lw    $fp, 40($sp)
    lw    $s0, 36($sp)
    addiu $sp, $sp, 48
    jr    $ra 
    nop

