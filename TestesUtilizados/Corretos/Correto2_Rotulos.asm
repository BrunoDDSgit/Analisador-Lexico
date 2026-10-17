.data
inicioMensagem: .asciiz "Inicio"
fimMensagem: .asciiz "Fim"

.text
Inicio:
li $t0, 10
beq $t0, $zero, Fim
j Inicio

Fim:
li $v0, 10
syscall