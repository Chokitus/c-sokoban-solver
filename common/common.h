#include "structures.h"

#ifndef _COMMON_H_
#define _COMMON_H_

// Função que imprime o caminho para a solução
extern void printPath(State *s);

// Função que usaremos intensivamente para mover o sokoban
extern char movePlayer(State *s, int dir, unsigned char (*getStateId)(State *));

// Função para construir o grid
extern void placeThis(char c, int x, int y, State *s);

// Função para construir o mapa
extern void buildMap(State *s, char *level);

// Função que verifica se o estado é final
// Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em
// mente, este não está preparado para níveis que possuam mais caixas que
// objetivos
extern unsigned char isFinal(State *s);

#endif