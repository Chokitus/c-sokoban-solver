#include "../common/common.h"
#include "../common/sort.h"
#include "../common/structures.h"
#include "../common/util.h"
#include "hash-table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Array para o espalhamento dos IDs.
VisitedId *idList[ID_HASH];

unsigned char getStateId(State *s) { return checkIfStateIdExists(idList, s); }

unsigned char insertState(Node **root, Node **last, State *s) {
	if (isFinal(s)) {
		//É final
		return 1;
	}

	int h = s->heuristic;

	if ((*root) == NULL) {
		// Criamos o nó
		(*root) = (Node *)malloc(sizeof(Node));
		(*root)->nextState = NULL;
		// Colocamos o estado no nó
		(*root)->state = (State *)malloc(sizeof(State));
		copyState(s, (*root)->state);
		return 0;
	}

	Node **next = &((*root)->nextState);

	if ((*next) == NULL) {
		// Se o próximo é vazio, nossa lista só tem um nó
		(*next) = (Node *)malloc(sizeof(Node));
		(*next)->nextState = NULL;
		(*next)->state = (State *)malloc(sizeof(State));
		// Se a heurística deste nó é menor que a do primeiro nó, trocamos.
		if (h < (*root)->state->heuristic) {
			copyState((*root)->state, (*next)->state);
			copyState(s, (*root)->state);
		} else {
			// Colocamos o estado no nó
			copyState(s, (*next)->state);
			return 0;
		}
	}

	// Sabemos que há mais de 2 nós
	next = &((*root));

	while ((*next)->state->heuristic <= h) {
		// Enquanto a heurística do nó for menor ou igual, vamos avançando
		next = &((*next)->nextState);
		if ((*next) == NULL) {
			// Chegamos no final da lista.
			(*next) = (Node *)malloc(sizeof(Node));
			(*next)->state = (State *)malloc(sizeof(State));
			(*next)->nextState = NULL;
			copyState(s, (*next)->state);
			return 0;
		}
	}

	// Se chegamos aqui, há algum nó com uma heurística maior que o estado s, e
	// este está em next Criamos um novo nó.
	Node *transferState = (Node *)malloc(sizeof(Node));

	// Copiamos o nó com maior heurística para transferState
	memcpy(transferState, (*next), sizeof(Node));
	transferState->state = (State *)malloc(sizeof(State));

	// Copiamos o valor do estado para o próximo nó
	copyState((*next)->state, transferState->state);

	// Redirecionamos a lista para o próximo nó
	(*next)->nextState = transferState;

	// Copiamos o estado.
	copyState(s, (*next)->state);
	return 0;
}

int getHeuristic(State *s) {
	int min, x, y, xGoal, yGoal, dist, h = 0;

	// Para cada caixa
	for (int i = 0; i < 30 && s->posBoxes[i] != 0; i++) {
		// Começamos com um número grande que não será superado, visto que a
		// maior distância possível seria 2*20² = 800
		min = 1000;

		// Pegamos a posição x e y
		x = s->posBoxes[i] % 20;
		y = s->posBoxes[i] / 20;

		// Para cada goal
		for (int j = 0; j < 30 && s->posGoals[j] != 0; j++) {
			// Pegamos a posição x e y
			xGoal = s->posGoals[j] % 20;
			yGoal = s->posGoals[j] / 20;

			//"Distância" na forma (x-x')+(y-y'). Entre aspas pois o correto
			// seria a raiz do mesmo, mas isto não importa para nós
			dist = (x - xGoal) * (x - xGoal) + (y - yGoal) * (y - yGoal);

			if (dist < min) {
				// Devemos tomar a mínima das distâncias, pois níveis com goals
				// muito separados nos dariam problemas
				if (dist == 0) {
					// Se min == 0, então a caixa está em um goal, então é
					// inútil procurar nos demais.
					break;
				}
				if (s->grid[s->posGoals[j]] != FILLED_GOAL) {
					// Faremos essa atribuição para o min somente se o goal que
					// estamos olhando não possui uma caixa sobre ele. Como esta
					// caixa não entrou em dist == 0, significa que se uma caixa
					// está sobre o goal, não é a que estamos olhando.
					min = dist;
				}
			}
		}

		// Nossa heurística é, assim, a somatória das distâncias mínimas das
		// caixas aos objetivos.
		h += min;
	}

	// Dividimos a mesma pela quantidade de caixas nos alvos para que esta ação
	// seja recompensada
	s->heuristic = h / (s->boxesOnGoals + 1);
	return h;
};

int main(int argc, char *argv[]) {
	Node *root = (Node *)malloc(sizeof(Node));
	State *s, *rootState;
	unsigned char final = 0;

	root->state = (State *)malloc(sizeof(State));
	root->nextState = NULL;

	// Apontamos cada um dos Ids para nulo.
	memset(idList, 0, ID_HASH);

	// Inicializamos o ponteiro para o último estado
	Node **last = (Node **)malloc(sizeof(Node *));
	(*last) = NULL;

	buildMap(root->state, argv[1]);
	getStateId(root->state);
	getHeuristic(root->state);

	s = (State *)malloc(sizeof(State));

	while (final != 1) {
		rootState = popState(&root);

		for (int i = 0; i < 4; i++) {
			copyState(rootState, s);
			if (movePlayer(s, i, getStateId) != 0) {
				getHeuristic(s);
				final = insertState(&root, last, s);
			}

			if (final == 1) {
				printf("Achei a solução!\n");
				printPath(s);
				break;
			}
		}
		free(rootState);
	}

	return 0;
}
