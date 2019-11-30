
#include "util.h"

#ifndef _STRUCTURES_H_
#define _STRUCTURES_H_

// Ligada ligada para as ações
typedef struct ActionList {
	unsigned char action;
	struct ActionList *prevAction;
} ActionList;

// Estado que estaremos analisando
typedef struct State {
	// Todas as posições se baseiam no fato que não há mapa com mais de 19 de
	// largura/altura. Portanto, a posição é 20*y+x; Posição do Sokoban
	unsigned short posPlayer;

	// Posição das caixas
	unsigned short posBoxes[30];

	// ?
	unsigned short posGoals[30];

	// Mapa, usado para facilitar no movimento.
	unsigned char grid[400];

	// Quantidade de caixas encontradas
	unsigned char boxes;

	// Grid dimensions
	unsigned int width;
	unsigned int height;

	// Quantidade de caixas no objetivo
	unsigned char boxesOnGoals;

	// Caminho para a solução
	ActionList *lastAction;

	// Próximo Estado, considerando que isto também é uma lista ligada
	struct State *nextState;

	unsigned int heuristic;
} State;

#endif