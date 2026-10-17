#ifndef PREPROCESSADOR_H
#define PREPROCESSADOR_H

#include <stdio.h>

void TrocarExtensao(const char *origem, const char *novaExtensao, char *destino, size_t tamanhoDestino);
int PreProcessamento(FILE *in, FILE *out);

#endif