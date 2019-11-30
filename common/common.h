#include "structures.h"

#ifndef _COMMON_H_
#define _COMMON_H_

// Função que imprime o caminho para a solução
extern void printPath(State *s);

// Função que usaremos intensivamente para mover o sokoban
extern char movePlayer(State *s, int dir, unsigned char (*getStateId)(State *));

// Função para construir o grid
extern void placeThis(char c, int x, int y, State *s);

#endif