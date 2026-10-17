.data
mensagem: .asciiz "Teste de comentario" # comentario depois da string
valor: .word 10 # valor inicial

.text
main:
li $t0, 5 # carregando valor
add $t1, $t0, $t0 # realizando soma
# esta linha inteira e um comentario
syscall # finalizacao