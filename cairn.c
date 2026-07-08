/*
 * cairn - a tiny stack-based esoteric language interpreter.
 *
 * A cairn is a stack of stones left to mark a trail. This language is built
 * the same way: you stack values and knock them down. See README.md for the
 * full language reference.
 *
 * Build: cc -O2 -o cairn cairn.c
 * Run:   ./cairn program.cairn      (or pipe a program on stdin)
 *
 * Copyright (c) 2026 neverlone. MIT License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- opcodes ------------------------------------------------------------ */
enum {
    OP_PUSH, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_PRN, OP_PRC, OP_DUP, OP_DROP, OP_SWAP, OP_OVER, OP_ROT,
    OP_EQ, OP_LT, OP_GT, OP_NOT, OP_WHILE, OP_WEND, OP_GET, OP_PUT
};

typedef struct { int op; long long arg; } Insn;

/* ---- fatal error -------------------------------------------------------- */
static void die(const char *msg) {
    fprintf(stderr, "cairn: %s\n", msg);
    exit(1);
}

/* ---- source loading ----------------------------------------------------- */
static char *slurp(const char *path) {
    FILE *f = path ? fopen(path, "rb") : stdin;
    if (!f) die("cannot open source file");
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) die("out of memory");
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) die("out of memory"); }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    if (path) fclose(f);
    return buf;
}

/* ---- tokenizer: source text -> instruction array ------------------------ */
/*
 * Whitespace and any unrecognized character are ignored, so free-form
 * spacing works. '(' ... ')' is a comment. A run of digits is a number
 * literal. "..." pushes a 0 terminator then the characters in reverse, so a
 * `[@]` loop prints the string left-to-right and stops on the 0.
 */
static Insn *tokenize(const char *src, size_t *out_n) {
    size_t cap = 64, n = 0;
    Insn *code = malloc(cap * sizeof *code);
    if (!code) die("out of memory");
    #define EMIT(o, a) do { \
        if (n >= cap) { cap *= 2; code = realloc(code, cap * sizeof *code); if (!code) die("out of memory"); } \
        code[n].op = (o); code[n].arg = (a); n++; \
    } while (0)

    for (size_t i = 0; src[i]; ) {
        char c = src[i];
        if (c == '(') {                               /* comment, may nest */
            i++;
            int depth = 1;
            while (src[i] && depth > 0) {
                if (src[i] == '(') depth++;
                else if (src[i] == ')') depth--;
                i++;
            }
            if (depth > 0) die("unterminated ( comment");
        } else if (c >= '0' && c <= '9') {            /* number literal */
            long long v = 0;
            while (src[i] >= '0' && src[i] <= '9') { v = v * 10 + (src[i] - '0'); i++; }
            EMIT(OP_PUSH, v);
        } else if (c == '"') {                         /* string literal */
            i++;
            char tmp[4096]; size_t tn = 0;
            while (src[i] && src[i] != '"') {
                char ch = src[i++];
                if (ch == '\\' && src[i]) {            /* escapes */
                    char e = src[i++];
                    ch = e == 'n' ? '\n' : e == 't' ? '\t' :
                         e == '0' ? '\0' : e;          /* \\ and \" fall through */
                }
                if (tn >= sizeof tmp) die("string literal too long");
                tmp[tn++] = ch;
            }
            if (!src[i]) die("unterminated \" string");
            i++;
            EMIT(OP_PUSH, 0);                          /* terminator, printed last */
            for (size_t k = tn; k > 0; k--) EMIT(OP_PUSH, (unsigned char)tmp[k - 1]);
        } else {
            int op = -1;
            switch (c) {
                case '+': op = OP_ADD;  break;
                case '-': op = OP_SUB;  break;
                case '*': op = OP_MUL;  break;
                case '/': op = OP_DIV;  break;
                case '%': op = OP_MOD;  break;
                case '.': op = OP_PRN;  break;
                case '@': op = OP_PRC;  break;
                case ':': op = OP_DUP;  break;
                case '$': op = OP_DROP; break;
                case '\\':op = OP_SWAP; break;
                case 'o': op = OP_OVER; break;
                case 'r': op = OP_ROT;  break;
                case '=': op = OP_EQ;   break;
                case '<': op = OP_LT;   break;
                case '>': op = OP_GT;   break;
                case '!': op = OP_NOT;  break;
                case '[': op = OP_WHILE;break;
                case ']': op = OP_WEND; break;
                case 'g': op = OP_GET;  break;
                case 'p': op = OP_PUT;  break;
                default:  op = -1;      break;         /* ignore everything else */
            }
            if (op >= 0) EMIT(op, 0);
            i++;
        }
    }
    #undef EMIT
    *out_n = n;
    return code;
}

/* ---- match [ ] into a jump table ---------------------------------------- */
static long long *match_brackets(const Insn *code, size_t n) {
    long long *match = malloc(n * sizeof *match);
    size_t *stk = malloc((n + 1) * sizeof *stk);
    if (!match || !stk) die("out of memory");
    size_t sp = 0;
    for (size_t i = 0; i < n; i++) {
        if (code[i].op == OP_WHILE) stk[sp++] = i;
        else if (code[i].op == OP_WEND) {
            if (sp == 0) die("unmatched ]");
            size_t j = stk[--sp];
            match[i] = (long long)j;
            match[j] = (long long)i;
        }
    }
    if (sp != 0) die("unmatched [");
    free(stk);
    return match;
}

/* ---- the machine -------------------------------------------------------- */
#define STACK_MAX 65536
#define FIELD_MAX 256

int main(int argc, char **argv) {
    char *src = slurp(argc >= 2 ? argv[1] : NULL);
    size_t n;
    Insn *code = tokenize(src, &n);
    long long *match = match_brackets(code, n);

    long long *stk = malloc(STACK_MAX * sizeof *stk);
    long long field[FIELD_MAX] = {0};
    if (!stk) die("out of memory");
    long long sp = 0;

    #define NEED(k) do { if (sp < (k)) die("stack underflow"); } while (0)
    #define PUSH(v) do { if (sp >= STACK_MAX) die("stack overflow"); stk[sp++] = (v); } while (0)

    for (size_t ip = 0; ip < n; ip++) {
        long long a, b;
        switch (code[ip].op) {
        case OP_PUSH: PUSH(code[ip].arg); break;
        case OP_ADD:  NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a + b); break;
        case OP_SUB:  NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a - b); break;
        case OP_MUL:  NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a * b); break;
        case OP_DIV:  NEED(2); b = stk[--sp]; a = stk[--sp]; if (!b) die("division by zero"); PUSH(a / b); break;
        case OP_MOD:  NEED(2); b = stk[--sp]; a = stk[--sp]; if (!b) die("modulo by zero"); PUSH(a % b); break;
        case OP_PRN:  NEED(1); printf("%lld", stk[--sp]); break;
        case OP_PRC:  NEED(1); putchar((int)(stk[--sp] & 0xFF)); break;
        case OP_DUP:  NEED(1); a = stk[sp - 1]; PUSH(a); break;
        case OP_DROP: NEED(1); sp--; break;
        case OP_SWAP: NEED(2); a = stk[sp - 1]; stk[sp - 1] = stk[sp - 2]; stk[sp - 2] = a; break;
        case OP_OVER: NEED(2); a = stk[sp - 2]; PUSH(a); break;
        case OP_ROT:  NEED(3); a = stk[sp - 3]; stk[sp - 3] = stk[sp - 2]; stk[sp - 2] = stk[sp - 1]; stk[sp - 1] = a; break;
        case OP_EQ:   NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a == b); break;
        case OP_LT:   NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a < b); break;
        case OP_GT:   NEED(2); b = stk[--sp]; a = stk[--sp]; PUSH(a > b); break;
        case OP_NOT:  NEED(1); a = stk[--sp]; PUSH(a == 0); break;
        case OP_WHILE: NEED(1); if (stk[sp - 1] == 0) ip = (size_t)match[ip]; break;
        case OP_WEND:  ip = (size_t)match[ip] - 1; break;
        case OP_GET:  NEED(1); a = stk[--sp]; if (a < 0 || a >= FIELD_MAX) die("field address out of range"); PUSH(field[a]); break;
        case OP_PUT:  NEED(2); a = stk[--sp]; b = stk[--sp]; if (a < 0 || a >= FIELD_MAX) die("field address out of range"); field[a] = b; break;
        }
    }

    free(src); free(code); free(match); free(stk);
    return 0;
}
