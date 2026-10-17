#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "preprocessador.h"
#include "lexico.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso correto: %s <entrada.asm> <saida.lex>\n", argv[0]);
        return 1;
    }

    char nomePre[512];
    TrocarExtensao(argv[1], ".pre", nomePre, sizeof(nomePre));

    /* ---------- Etapa 1: Pre-processamento ---------- */

    FILE *arqEntrada = fopen(argv[1], "r");
    if (!arqEntrada) {
        printf("Erro ao abrir o arquivo de entrada: %s\n", argv[1]);
        return 1;
    }

    FILE *arqPreEscrita = fopen(nomePre, "w");
    if (!arqPreEscrita) {
        printf("Erro ao abrir o arquivo de pre-processamento para escrita: %s\n", nomePre);
        fclose(arqEntrada);
        return 1;
    }

    int statusPre = PreProcessamento(arqEntrada, arqPreEscrita);

    fclose(arqEntrada);
    fclose(arqPreEscrita);

    if (statusPre != 0) {
        printf("Erro durante o pre-processamento do arquivo.\n");
        return 1;
    }

    /* ---------- Etapa 2: Analise lexica ---------- */

    FILE *arqPreLeitura = fopen(nomePre, "r");
    if (!arqPreLeitura) {
        printf("Erro ao reabrir o arquivo pre-processado para leitura: %s\n", nomePre);
        return 1;
    }

    FILE *arqLex = fopen(argv[2], "w");
    if (!arqLex) {
        printf("Erro ao abrir o arquivo de saida lexica: %s\n", argv[2]);
        fclose(arqPreLeitura);
        return 1;
    }

    char nomeTS[512];
    char nomeErr[512];
    TrocarExtensao(argv[2], ".ts", nomeTS, sizeof(nomeTS));
    TrocarExtensao(argv[2], ".err", nomeErr, sizeof(nomeErr));

    if (DefinirArquivoSaidaTS(nomeTS) != 0) {
        printf("Erro ao abrir o arquivo da Tabela de Simbolos: %s\n", nomeTS);
        fclose(arqPreLeitura);
        fclose(arqLex);
        return 1;
    }

    if (DefinirArquivoSaidaErros(nomeErr) != 0) {
        printf("Erro ao abrir o arquivo de erros lexicos: %s\n", nomeErr);
        fclose(arqPreLeitura);
        fclose(arqLex);
        return 1;
    }

    AnaliseLexica(arqPreLeitura, arqLex);

    fclose(arqPreLeitura);
    fclose(arqLex);

    printf("Processamento concluido com sucesso!\n");
    printf("  Pre-processado: %s\n", nomePre);
    printf("  Tokens (.lex):  %s\n", argv[2]);
    printf("  Tabela (.ts):   %s\n", nomeTS);
    printf("  Erros (.err):   %s\n", nomeErr);

    return 0;
}