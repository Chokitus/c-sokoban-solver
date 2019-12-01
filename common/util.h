#include <string.h>

#ifndef _UTIL_H_
#define _UTIL_H_

#define copyState(s, s2) memcpy(s2, s, sizeof(State))
#define NANOS ((unsigned long)1000000000)

// Tamanho máximo do caminho
#define MAX_PATH_SIZE 3000

enum Direcoes { direita, esquerda, cima, baixo };

#endif