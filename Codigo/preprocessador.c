#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"

void TrocarExtensao(const char *origem, const char *novaExtensao,
                     char *destino, size_t tamanhoDestino) {
    const char *ponto = strrchr(origem, '.');
    size_t tamanhoBase = ponto ? (size_t) (ponto - origem) : strlen(origem);
    if (tamanhoBase >= tamanhoDestino) tamanhoBase = tamanhoDestino - 1;

    strncpy(destino, origem, tamanhoBase);
    destino[tamanhoBase] = '\0';
    strncat(destino, novaExtensao, tamanhoDestino - strlen(destino) - 1);
}

/* Le uma linha inteira do arquivo, sem limite fixo de tamanho. */
static char *ler_linha(FILE *f, int *erro_alocacao) {
    size_t capacidade = 256;
    size_t tamanho = 0;
    char *buffer = malloc(capacidade);
    if (!buffer) {
        *erro_alocacao = 1;
        return NULL;
    }

    int c;
    int leu_algo = 0;

    while ((c = fgetc(f)) != EOF) {
        leu_algo = 1;

        if (tamanho + 1 >= capacidade) {
            capacidade *= 2;
            char *novo = realloc(buffer, capacidade);
            if (!novo) {
                free(buffer);
                *erro_alocacao = 1;
                return NULL;
            }
            buffer = novo;
        }

        if (c == '\n') {
            buffer[tamanho] = '\0';
            return buffer;
        }

        buffer[tamanho++] = (char) c;
    }

    if (!leu_algo) {
        free(buffer);
        return NULL;
    }

    buffer[tamanho] = '\0';
    return buffer;
}

int PreProcessamento(FILE *in, FILE *out) {
    char *linha;
    int erro_alocacao = 0;

    while ((linha = ler_linha(in, &erro_alocacao)) != NULL) {
        /* Normaliza CRLF -> LF removendo um eventual '\r' no final da linha */
        size_t len = strlen(linha);
        if (len > 0 && linha[len - 1] == '\r') {
            linha[len - 1] = '\0';
            len--;
        }

        char *limpa = malloc(len + 1);
        if (!limpa) {
            free(linha);
            return -1;
        }

        size_t i = 0, j = 0;
        int espaco = 0;
        int dentro_string = 0;

        while (linha[i] == ' ' || linha[i] == '\t') i++;

        while (linha[i] != '\0') {
            char c = linha[i];

            if (!dentro_string && c == '#') break;

            if (c == '"') {
                dentro_string = !dentro_string;
                limpa[j++] = c;
                espaco = 0;
                i++;
                continue;
            }

            if (dentro_string) {
                limpa[j++] = c;
                i++;
                continue;
            }

            if (c == ' ' || c == '\t') {
                if (!espaco) {
                    limpa[j++] = ' ';
                    espaco = 1;
                }
            } else {
                limpa[j++] = c;
                espaco = 0;
            }
            i++;
        }

        if (!dentro_string && j > 0 && limpa[j - 1] == ' ') j--;
        limpa[j] = '\0';

        if (j > 0) fprintf(out, "%s\n", limpa);

        free(limpa);
        free(linha);
    }

    if (erro_alocacao) return -1;
    return 0;
}