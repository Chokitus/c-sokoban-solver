#ifndef _UTIL_H_
#define _UTIL_H_

#define copyState(s, s2) memcpy(s2, s, sizeof(State))
#define NANOS ((unsigned long)1000000000)
enum Direcoes { direita, esquerda, cima, baixo };

#endif