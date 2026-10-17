#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexico.h"

/* ========================================================================
 * ANALISADOR LEXICO - AFD
 *
 *
 * q0  = estado inicial
 * qID = identificador/instrucao
 * qDIR = diretiva
 * qREG = registrador
 * qNUM = numero decimal
 * qHEX = numero hexadecimal
 * qNEG = inicio de numero negativo
 * qSTR = cadeia de caracteres
 * qESC = escape dentro de cadeia
 * qCOM = comentario
 * qPONT = pontuacao
 * qFIM = estado de aceitacao/retorno
 * qERR = erro lexical
 *
 * A classificacao final de instrucoes, diretivas e registradores e feita
 * apos o AFD reconhecer a forma lexica correspondente.
 * ======================================================================== */

typedef struct {
    const char *lexema;
    const char *token;
} PalavraReservada;

typedef enum {
    AFD_INICIAL,
    AFD_IDENTIFICADOR,
    AFD_DIRETIVA,
    AFD_REGISTRADOR,
    AFD_NUMERO,
    AFD_NEGATIVO,
    AFD_HEX,
    AFD_STRING,
    AFD_ESCAPE,
    AFD_COMENTARIO,
    AFD_PONTUACAO,
    AFD_FINAL,
    AFD_ERRO
} EstadoAFD;

/* ========================================================================
 * Tabela de Simbolos
 * ======================================================================== */

static Simbolo *tabela = NULL;
static int tabelaTamanho = 0;
static int tabelaCapacidade = 0;

static FILE *arqTS = NULL;
static FILE *arqErros = NULL;
static int totalErros = 0;

static int linhaAtual = 1;
static int colunaAtual = 1;

// palavras reservadas

static const PalavraReservada INSTRUCOES[] = {
    {"li", "INS_LI"}, {"la", "INS_LA"}, {"move", "INS_MOVE"},
    {"lw", "INS_LW"}, {"sw", "INS_SW"}, {"lb", "INS_LB"},
    {"sb", "INS_SB"}, {"lh", "INS_LH"}, {"sh", "INS_SH"},
    {"add", "INS_ADD"}, {"addi", "INS_ADDI"}, {"addu", "INS_ADDU"},
    {"sub", "INS_SUB"}, {"subu", "INS_SUBU"}, {"mul", "INS_MUL"},
    {"div", "INS_DIV"}, {"mflo", "INS_MFLO"}, {"mfhi", "INS_MFHI"},
    {"and", "INS_AND"}, {"or", "INS_OR"}, {"xor", "INS_XOR"},
    {"nor", "INS_NOR"}, {"sll", "INS_SLL"}, {"srl", "INS_SRL"},
    {"slt", "INS_SLT"}, {"beq", "INS_BEQ"}, {"bne", "INS_BNE"},
    {"blt", "INS_BLT"}, {"ble", "INS_BLE"}, {"bgt", "INS_BGT"},
    {"bge", "INS_BGE"}, {"j", "INS_J"}, {"jal", "INS_JAL"},
    {"jr", "INS_JR"}, {"syscall", "INS_SYSCALL"}, {"nop", "INS_NOP"}
};

static const int TOTAL_INSTRUCOES =
    (int)(sizeof(INSTRUCOES) / sizeof(INSTRUCOES[0]));

static const PalavraReservada DIRETIVAS[] = {
    {".data", "DIR_DATA"}, {".text", "DIR_TEXT"}, {".word", "DIR_WORD"},
    {".half", "DIR_HALF"}, {".byte", "DIR_BYTE"}, {".space", "DIR_SPACE"},
    {".ascii", "DIR_ASCII"}, {".asciiz", "DIR_ASCIIZ"},
    {".globl", "DIR_GLOBL"}, {".align", "DIR_ALIGN"}
};

static const int TOTAL_DIRETIVAS =
    (int)(sizeof(DIRETIVAS) / sizeof(DIRETIVAS[0]));

static const char *REGISTRADORES_NOMEADOS[] = {
    "$zero", "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8", "$t9",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

static const int TOTAL_REGISTRADORES =
    (int)(sizeof(REGISTRADORES_NOMEADOS) / sizeof(REGISTRADORES_NOMEADOS[0]));


// simbolos

static void GarantirCapacidadeTabela(void) {
    if (tabelaTamanho < tabelaCapacidade) return;

    tabelaCapacidade = (tabelaCapacidade == 0) ? 64 : tabelaCapacidade * 2;
    tabela = (Simbolo *)realloc(
        tabela,
        (size_t)tabelaCapacidade * sizeof(Simbolo)
    );

    if (!tabela) {
        fprintf(stderr, "Erro fatal: falha de alocacao na Tabela de Simbolos.\n");
        exit(1);
    }
}

static int BuscarNaTabela(const char *lexema) {
    for (int i = 0; i < tabelaTamanho; i++) {
        if (strcmp(tabela[i].lexema, lexema) == 0) return i;
    }
    return -1;
}

static void InserirSimbolo(const char *lexema, const char *categoria,
                           int linha, int coluna) {
    if (BuscarNaTabela(lexema) != -1) return;

    GarantirCapacidadeTabela();

    Simbolo *s = &tabela[tabelaTamanho++];
    strncpy(s->lexema, lexema, sizeof(s->lexema) - 1);
    s->lexema[sizeof(s->lexema) - 1] = '\0';

    strncpy(s->categoria, categoria, sizeof(s->categoria) - 1);
    s->categoria[sizeof(s->categoria) - 1] = '\0';

    s->primeiraLinha = linha;
    s->primeiraColuna = coluna;
}

static void AtualizarPrimeiraOcorrencia(const char *lexema,
                                        int linha, int coluna) {
    int idx = BuscarNaTabela(lexema);

    if (idx != -1 &&
        tabela[idx].primeiraLinha == 0 &&
        tabela[idx].primeiraColuna == 0) {
        tabela[idx].primeiraLinha = linha;
        tabela[idx].primeiraColuna = coluna;
    }
}

static void InicializarTabelaSimbolos(void) {
    for (int i = 0; i < TOTAL_INSTRUCOES; i++)
        InserirSimbolo(INSTRUCOES[i].lexema, "instrução", 0, 0);

    for (int i = 0; i < TOTAL_DIRETIVAS; i++)
        InserirSimbolo(DIRETIVAS[i].lexema, "diretiva", 0, 0);

    for (int i = 0; i < TOTAL_REGISTRADORES; i++)
        InserirSimbolo(REGISTRADORES_NOMEADOS[i], "registrador", 0, 0);
}

static void GravarTabelaSimbolos(void) {
    fprintf(arqTS, "LEXEMA,CATEGORIA,LINHA,COLUNA\n");

    for (int i = 0; i < tabelaTamanho; i++) {
        fprintf(arqTS, "%s,%s,%d,%d\n",
                tabela[i].lexema,
                tabela[i].categoria,
                tabela[i].primeiraLinha,
                tabela[i].primeiraColuna);
    }
}

// palavras reservadqs e registradores

static const char *BuscarInstrucao(const char *lexema) {
    for (int i = 0; i < TOTAL_INSTRUCOES; i++) {
        if (strcmp(INSTRUCOES[i].lexema, lexema) == 0)
            return INSTRUCOES[i].token;
    }
    return NULL;
}

static const char *BuscarDiretiva(const char *lexema) {
    for (int i = 0; i < TOTAL_DIRETIVAS; i++) {
        if (strcmp(DIRETIVAS[i].lexema, lexema) == 0)
            return DIRETIVAS[i].token;
    }
    return NULL;
}

static int RegistradorValido(const char *lexema) {
    for (int i = 0; i < TOTAL_REGISTRADORES; i++) {
        if (strcmp(REGISTRADORES_NOMEADOS[i], lexema) == 0)
            return 1;
    }

    if (lexema[0] != '$' || lexema[1] == '\0') return 0;

    for (int i = 1; lexema[i] != '\0'; i++) {
        if (!isdigit((unsigned char)lexema[i])) return 0;
    }

    int valor = atoi(lexema + 1);
    return valor >= 0 && valor <= 31;
}

//leitura/posição

static int ConsumirChar(FILE *in) {
    int c = fgetc(in);

    if (c != EOF) {
        if (c == '\n') {
            linhaAtual++;
            colunaAtual = 1;
        } else {
            colunaAtual++;
        }
    }

    return c;
}

static int Espiar(FILE *in) {
    int c = fgetc(in);

    if (c != EOF) ungetc(c, in);
    return c;
}

//tokens e erros

static void GravarErro(const char *tipo, const char *lexema,
                       int linha, int coluna) {
    fprintf(arqErros, "<%s, %s> %d %d\n",
            tipo, lexema, linha, coluna);
    totalErros++;
}

static void MontarToken(Token *tok, const char *nome, const char *lexema,
                        int linha, int coluna) {
    strncpy(tok->nome, nome, sizeof(tok->nome) - 1);
    tok->nome[sizeof(tok->nome) - 1] = '\0';

    strncpy(tok->lexema, lexema, sizeof(tok->lexema) - 1);
    tok->lexema[sizeof(tok->lexema) - 1] = '\0';

    tok->linha = linha;
    tok->coluna = coluna;
}

static void NormalizarMinusculo(char *texto) {
    for (int i = 0; texto[i] != '\0'; i++) {
        texto[i] = (char)tolower((unsigned char)texto[i]);
    }
}

static Token AFD_Identificador(FILE *in, int linhaTok, int colunaTok) {
    Token tok;
    char buf[100];
    int n = 0;
    EstadoAFD estado = AFD_IDENTIFICADOR;

    while (estado == AFD_IDENTIFICADOR) {
        int c = Espiar(in);

        if (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
            ConsumirChar(in);

            if (n < (int)sizeof(buf) - 1) {
                buf[n++] = (char)c;
            } else {
                estado = AFD_ERRO;
            }
        } else {
            estado = AFD_FINAL;
        }
    }

    buf[n] = '\0';

    int c = Espiar(in);

    if (estado == AFD_ERRO || c == '@' || c == '-' || c == '$' || c == '.') {
        while ((c = Espiar(in)) != EOF &&
               c != ' ' && c != '\t' && c != '\n' && c != '\r' &&
               c != ',' && c != ':' && c != '(' && c != ')') {
            ConsumirChar(in);

            if (n < (int)sizeof(buf) - 1)
                buf[n++] = (char)c;
        }

        buf[n] = '\0';
        MontarToken(&tok, "ERRO_IDENTIFICADOR_MALFORMADO",
                    buf, linhaTok, colunaTok);
        return tok;
    }

    char normalizado[100];
    strncpy(normalizado, buf, sizeof(normalizado) - 1);
    normalizado[sizeof(normalizado) - 1] = '\0';
    NormalizarMinusculo(normalizado);

    const char *tkReservado = BuscarInstrucao(normalizado);

    if (tkReservado != NULL) {
        AtualizarPrimeiraOcorrencia(normalizado, linhaTok, colunaTok);
        MontarToken(&tok, tkReservado, normalizado, linhaTok, colunaTok);
    } else {
        /* Identificadores/rótulos preservam a capitalizacao. */
        MontarToken(&tok, "ID", buf, linhaTok, colunaTok);
    }

    return tok;
}

static Token AFD_Diretiva(FILE *in, int linhaTok, int colunaTok) {
    Token tok;
    char buf[64];
    int n = 0;
    EstadoAFD estado = AFD_DIRETIVA;

    buf[n++] = (char)ConsumirChar(in); /* '.' */

    while (estado == AFD_DIRETIVA) {
        int c = Espiar(in);

        if (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
            ConsumirChar(in);

            if (n < (int)sizeof(buf) - 1)
                buf[n++] = (char)c;
            else
                estado = AFD_ERRO;
        } else {
            estado = AFD_FINAL;
        }
    }

    buf[n] = '\0';

    char normalizado[64];
    strncpy(normalizado, buf, sizeof(normalizado) - 1);
    normalizado[sizeof(normalizado) - 1] = '\0';
    NormalizarMinusculo(normalizado);

    const char *tkDiretiva = BuscarDiretiva(normalizado);

    if (tkDiretiva != NULL && estado != AFD_ERRO) {
        AtualizarPrimeiraOcorrencia(normalizado, linhaTok, colunaTok);
        MontarToken(&tok, tkDiretiva, normalizado, linhaTok, colunaTok);
    } else {
        MontarToken(&tok, "ERRO_DIRETIVA_INVALIDA",
                    buf, linhaTok, colunaTok);
    }

    return tok;
}

static Token AFD_Registrador(FILE *in, int linhaTok, int colunaTok) {
    Token tok;
    char buf[32];
    int n = 0;
    EstadoAFD estado = AFD_REGISTRADOR;

    buf[n++] = (char)ConsumirChar(in); /* '$' */

    while (estado == AFD_REGISTRADOR) {
        int c = Espiar(in);

        if (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
            ConsumirChar(in);

            if (n < (int)sizeof(buf) - 1)
                buf[n++] = (char)c;
            else
                estado = AFD_ERRO;
        } else {
            estado = AFD_FINAL;
        }
    }

    buf[n] = '\0';

    char normalizado[32];
    strncpy(normalizado, buf, sizeof(normalizado) - 1);
    normalizado[sizeof(normalizado) - 1] = '\0';
    NormalizarMinusculo(normalizado);

    if (estado != AFD_ERRO && RegistradorValido(normalizado)) {
        AtualizarPrimeiraOcorrencia(normalizado, linhaTok, colunaTok);
        MontarToken(&tok, "REG", normalizado, linhaTok, colunaTok);
    } else {
        MontarToken(&tok, "ERRO_REGISTRADOR_INVALIDO",
                    buf, linhaTok, colunaTok);
    }

    return tok;
}

static Token AFD_Numero(FILE *in, int linhaTok, int colunaTok, int negativo) {
    Token tok;
    char buf[64];
    int n = 0;
    int ehHex = 0;
    EstadoAFD estado = negativo ? AFD_NEGATIVO : AFD_NUMERO;

    if (negativo)
        buf[n++] = '-';

    if (Espiar(in) == '0') {
        buf[n++] = (char)ConsumirChar(in);

        if (Espiar(in) == 'x' || Espiar(in) == 'X') {
            buf[n++] = (char)ConsumirChar(in);
            ehHex = 1;
            estado = AFD_HEX;
        } else {
            estado = AFD_NUMERO;
        }
    } else {
        estado = ehHex ? AFD_HEX : AFD_NUMERO;
    }

    while (estado == AFD_NUMERO || estado == AFD_HEX || estado == AFD_NEGATIVO) {
        int c = Espiar(in);

        if (c != EOF && (isalnum((unsigned char)c) || c == '_')) {
            ConsumirChar(in);

            if (n < (int)sizeof(buf) - 1)
                buf[n++] = (char)c;
        } else {
            estado = AFD_FINAL;
        }
    }

    buf[n] = '\0';

    const char *digitos = buf + (negativo ? 1 : 0) + (ehHex ? 2 : 0);
    int valido = (*digitos != '\0');

    for (const char *p = digitos; *p != '\0' && valido; p++) {
        if (ehHex) {
            if (!isxdigit((unsigned char)*p)) valido = 0;
        } else {
            if (!isdigit((unsigned char)*p)) valido = 0;
        }
    }

    if (valido) {
        MontarToken(&tok, "NUM_INT", buf, linhaTok, colunaTok);
    } else {
        MontarToken(&tok, "ERRO_NUMERO_MALFORMADO",
                    buf, linhaTok, colunaTok);
    }

    return tok;
}

static Token AFD_String(FILE *in, int linhaTok, int colunaTok) {
    Token tok;
    char buf[256];
    int n = 0;
    EstadoAFD estado = AFD_STRING;
    int escapeInvalido = 0;

    buf[n++] = (char)ConsumirChar(in); /* aspas iniciais */

    while (estado != AFD_FINAL && estado != AFD_ERRO) {
        int c = Espiar(in);

        switch (estado) {
            case AFD_STRING:
                if (c == EOF || c == '\n') {
                    estado = AFD_ERRO;
                    break;
                }

                ConsumirChar(in);

                if (c == '"') {
                    if (n < (int)sizeof(buf) - 1)
                        buf[n++] = '"';
                    estado = AFD_FINAL;
                } else if (c == '\\') {
                    if (n < (int)sizeof(buf) - 1)
                        buf[n++] = '\\';
                    estado = AFD_ESCAPE;
                } else {
                    if (n < (int)sizeof(buf) - 1)
                        buf[n++] = (char)c;
                }
                break;

            case AFD_ESCAPE:
                if (c == EOF || c == '\n') {
                    estado = AFD_ERRO;
                    break;
                }

                ConsumirChar(in);

                if (c == 'n' || c == 't' || c == '"' || c == '\\' || c == '0') {
                    if (n < (int)sizeof(buf) - 1)
                        buf[n++] = (char)c;
                    estado = AFD_STRING;
                } else {
                    char escBuf[3] = {'\\', (char)c, '\0'};
                    GravarErro("ERRO_SEQUENCIA_ESCAPE_INVALIDA",
                               escBuf, linhaAtual, colunaAtual - 1);
                    escapeInvalido = 1;
                    if (n < (int)sizeof(buf) - 1)
                        buf[n++] = (char)c;
                    estado = AFD_STRING;
                }
                break;

            default:
                estado = AFD_ERRO;
                break;
        }
    }

    buf[n] = '\0';

    if (estado == AFD_FINAL) {
        MontarToken(&tok, "STRING", buf, linhaTok, colunaTok);
    } else if (escapeInvalido) {
        /* Mantem o comportamento de recuperacao: o erro do escape ja foi
           gravado e a cadeia pode ser devolvida como STRING. */
        MontarToken(&tok, "ERRO_STRING_NAO_FECHADA", buf,
                    linhaTok, colunaTok);
    } else {
        MontarToken(&tok, "ERRO_STRING_NAO_FECHADA", buf,
                    linhaTok, colunaTok);
    }

    return tok;
}

static Token ProximoToken(FILE *in) {
    Token tok;
    memset(&tok, 0, sizeof(tok));

    EstadoAFD estado = AFD_INICIAL;

    /* Espacos e comentarios sao consumidos antes de iniciar o reconhecimento
       do proximo lexema. O pre-processador normalmente ja remove comentarios. */
    while (estado == AFD_INICIAL) {
        int c = Espiar(in);

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ConsumirChar(in);
            continue;
        }

        if (c == '#') {
            estado = AFD_COMENTARIO;
            while (estado == AFD_COMENTARIO) {
                c = Espiar(in);

                if (c == EOF || c == '\n') {
                    estado = AFD_INICIAL;
                } else {
                    ConsumirChar(in);
                }
            }
            continue;
        }

        break;
    }

    int c = Espiar(in);
    int linhaTok = linhaAtual;
    int colunaTok = colunaAtual;

    if (c == EOF) {
        MontarToken(&tok, "TK_EOF", "EOF", linhaTok, colunaTok);
        return tok;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        estado = AFD_IDENTIFICADOR;
        return AFD_Identificador(in, linhaTok, colunaTok);
    }

    if (c == '.') {
        estado = AFD_DIRETIVA;
        return AFD_Diretiva(in, linhaTok, colunaTok);
    }

    if (c == '$') {
        estado = AFD_REGISTRADOR;
        return AFD_Registrador(in, linhaTok, colunaTok);
    }

    if (isdigit((unsigned char)c)) {
        estado = AFD_NUMERO;
        return AFD_Numero(in, linhaTok, colunaTok, 0);
    }

    if (c == '-') {
        ConsumirChar(in);

        if (isdigit((unsigned char)Espiar(in))) {
            estado = AFD_NEGATIVO;
            return AFD_Numero(in, linhaTok, colunaTok, 1);
        }

        MontarToken(&tok, "ERRO_CARACTERE_INVALIDO",
                    "-", linhaTok, colunaTok);
        return tok;
    }

    if (c == '"') {
        estado = AFD_STRING;
        return AFD_String(in, linhaTok, colunaTok);
    }

    if (c == ',' || c == ':' || c == '(' || c == ')') {
        estado = AFD_PONTUACAO;
        ConsumirChar(in);

        if (c == ',')
            MontarToken(&tok, "SMB_COM", ",", linhaTok, colunaTok);
        else if (c == ':')
            MontarToken(&tok, "SMB_COL", ":", linhaTok, colunaTok);
        else if (c == '(')
            MontarToken(&tok, "SMB_OPA", "(", linhaTok, colunaTok);
        else
            MontarToken(&tok, "SMB_CPA", ")", linhaTok, colunaTok);

        estado = AFD_FINAL;
        return tok;
    }

    estado = AFD_ERRO;
    {
        char buf[2] = {(char)c, '\0'};
        ConsumirChar(in);
        MontarToken(&tok, "ERRO_CARACTERE_INVALIDO",
                    buf, linhaTok, colunaTok);
    }

    return tok;
}

int DefinirArquivoSaidaTS(const char *nomeArquivo) {
    arqTS = fopen(nomeArquivo, "w");
    return arqTS ? 0 : -1;
}

int DefinirArquivoSaidaErros(const char *nomeArquivo) {
    arqErros = fopen(nomeArquivo, "w");
    return arqErros ? 0 : -1;
}

void AnaliseLexica(FILE *in, FILE *out) {
    linhaAtual = 1;
    colunaAtual = 1;
    totalErros = 0;
    tabelaTamanho = 0;

    InicializarTabelaSimbolos();

    Token anterior;
    memset(&anterior, 0, sizeof(anterior));

    Token t;

    do {
        t = ProximoToken(in);

        /* ID seguido de ':' => declaracao de rotulo. */
        if (strcmp(t.nome, "SMB_COL") == 0 &&
            strcmp(anterior.nome, "ID") == 0) {
            InserirSimbolo(anterior.lexema,
                           "identificador/rótulo",
                           anterior.linha,
                           anterior.coluna);
        }

        /* Palavras reservadas nao podem ser usadas como rotulos. */
        if (strcmp(t.nome, "SMB_COL") == 0 &&
            strcmp(anterior.nome, "ID") != 0 &&
            strcmp(anterior.nome, "TK_EOF") != 0) {

            if (strncmp(anterior.nome, "INS_", 4) == 0 ||
                strncmp(anterior.nome, "DIR_", 4) == 0) {
                GravarErro("ERRO_ROTULO_RESERVADO",
                           anterior.lexema,
                           anterior.linha,
                           anterior.coluna);
            }
        }

        if (strncmp(t.nome, "ERRO_", 5) == 0) {
            /* Alguns erros ja podem ter sido gravados internamente, como
               escapes invalidos. Evita-se gravar duas vezes somente para
               esse caso especifico. */
            if (strcmp(t.nome, "ERRO_STRING_NAO_FECHADA") != 0 ||
                totalErros == 0) {
                GravarErro(t.nome, t.lexema, t.linha, t.coluna);
            }
        } else {
            fprintf(out, "<%s, %s> %d %d\n",
                    t.nome, t.lexema, t.linha, t.coluna);
        }

        anterior = t;

    } while (strcmp(t.nome, "TK_EOF") != 0);

    GravarTabelaSimbolos();

    if (totalErros == 0)
        fprintf(arqErros, "Nenhum erro léxico encontrado.\n");

    if (arqTS) {
        fclose(arqTS);
        arqTS = NULL;
    }

    if (arqErros) {
        fclose(arqErros);
        arqErros = NULL;
    }

    free(tabela);
    tabela = NULL;
    tabelaCapacidade = 0;
}
