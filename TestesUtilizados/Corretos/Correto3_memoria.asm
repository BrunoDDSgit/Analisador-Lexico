.data
vetor: .word 10

.text
main:
lw $t0, 0($sp)
lw $t1, 4($sp)
sw $t1, 8($sp)
lb $t2, 12($sp)
sb $t2, 16($sp)
li $v0, 10
syscall