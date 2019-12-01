#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Função que imprime o caminho para a solução
void printPath(State *s) {
	// Apontamos para a última ação tomada
	ActionList *tempAction = s->lastAction;

	// Criamos mais que o espaço necessário para qualquer solução
	unsigned char actions[10000];
	memset(actions, 0, 10000);

	int i;
	// Enquanto não chegamos ao final (que é nulo)
	for (i = 0; tempAction; i++) {
		actions[i] = tempAction->action;
		tempAction = tempAction->prevAction;
	}
	i--;

	// Printamos em ordem reversa
	for (; i >= 0; i--) {
		printf("%c", actions[i]);
	}
	printf("\n");
}

int checkWallsAt(State *s, unsigned int pos) {
	if (s->grid[pos] == WALL) {
		return 1;
	}
	return 0;
};

// Função que verifica se prendemos a caixa
/*
     #
    #$
    Procuramos "cantos" como este. Se a caixa está num canto, e este canto não é
   um objetivo, travamos a caixa Esta é uma posição exemplo que não seria
   explorada:
    #######
    #   @$# <- Note que a caixa não poderá ser movida
    #     #
    #. #  #
    #. $  #
    #.$$  #
    #.#  @#
    #######
*/
unsigned char boxTrapped(State *s, int pos) {
	int wallsUp = s->grid[pos - 20] == WALL,
	    wallsDown = s->grid[pos + 20] == WALL,
	    wallsLeft = s->grid[pos - 1] == WALL,
	    wallsRight = s->grid[pos + 1] == WALL;
	if (s->grid[pos] == EMPTY_GOAL || s->grid[pos] == FILLED_GOAL) {
		return 0;
	}
	if (wallsUp && wallsLeft) {
		return 1;
	}
	if (wallsUp && wallsRight) {
		return 1;
	}
	if (wallsDown && wallsLeft) {
		return 1;
	}
	if (wallsDown && wallsRight) {
		return 1;
	}

	return 0;
}

// Função para adicionar a ação ao caminho
void addPath(char c, State *s) {

	// Criamos uma nova ação
	ActionList *nextAction = malloc(sizeof(ActionList));

	// Esta ação recebe a ação causada pelo movePlayer
	nextAction->action = c;

	// E aponta para a ação anterior
	nextAction->prevAction = s->lastAction;

	// O estado agora aponta para a sua ação geradora.
	s->lastAction = nextAction;
};

// Função que usaremos intensivamente para mover o sokoban
// getStateId is part of the different implementations and that is why we used
// it as an argument here.
char movePlayer(State *s, int dir, unsigned char (*getStateId)(State *)) {
	// dir == 0 -> direita
	// dir == 1 -> esquerda
	// dir == 2 -> cima
	// dir == 3 -> baixo
	char c;
	unsigned char box = 0;
	int tempPos;
	int movingParam;
	switch (dir) {
	case direita:
		movingParam = 1;
		c = 'r';
		break;
	case esquerda:
		movingParam = -1;
		c = 'l';
		break;
	case cima:
		movingParam = -20;
		c = 'u';
		break;
	case baixo:
		movingParam = 20;
		c = 'd';
		break;
	}

	// Usamos o trecho de código para obter o parametro de movimento, assim como
	// o caracter. Como parâmetro de movimento, queremos transformar a visao 1D
	// do array no movimento 2D do personagem, ou seja, dado que cada linha
	// possui 20 colunas, "andar para cima" recua 20 colunas, por isso o
	// movingParam é -20. A mesma lógica se aplica para os outros

	tempPos = s->posPlayer + movingParam;

	if (checkWallsAt(s, tempPos)) {
		// Tem uma parede.
		return 0;
	}

	if (s->grid[tempPos] == BOX || s->grid[tempPos] == FILLED_GOAL) {
		// Tem uma caixa na direção
		if (checkWallsAt(s, tempPos + movingParam) == 1 ||
		    s->grid[tempPos + movingParam] == BOX ||
		    s->grid[tempPos + movingParam] == FILLED_GOAL) {
			// Tem uma parede ou caixa após a caixa.
			return 0;
		};
		// Deixa a letra maiuscula
		c -= 32;
		box = 1;
	}

	// Efetiva o movimento
	if (box != 0) {
		if (boxTrapped(s, tempPos + movingParam)) {
			return 0;
		}
		// Empurrou uma caixa
		if (s->grid[tempPos] == FILLED_GOAL) {
			// A caixa estava sobre um alvo
			s->grid[tempPos] = EMPTY_GOAL;
			s->boxesOnGoals--;
		} else {
			// Não estava sobre nada
			s->grid[tempPos] = EMPTY;
		}
		if (s->grid[tempPos + movingParam] == EMPTY_GOAL) {
			// A posição de destino da caixa é alvo
			s->grid[tempPos + movingParam] = FILLED_GOAL;
			s->boxesOnGoals++;
		} else {
			s->grid[tempPos + movingParam] = BOX;
		}
		// Move a caixa nas posições
		for (int i = 0; i < 30 && s->posBoxes[i] != 0; i++) {
			if (s->posBoxes[i] == tempPos) {
				s->posBoxes[i] += movingParam;
			}
		}
	}

	if (s->grid[s->posPlayer] == PLAYER_ON_GOAL) {
		// Sokoban tava em cima de um alvo
		s->grid[s->posPlayer] = EMPTY_GOAL;
	} else {
		// Sokoban não estava sobre nada
		s->grid[s->posPlayer] = EMPTY;
	}
	if (s->grid[tempPos] == EMPTY_GOAL) {
		// Sokoban está indo na direção de um alvo
		s->grid[tempPos] = PLAYER_ON_GOAL;
	} else {
		// Não havia nada sobre a posição de destino
		s->grid[tempPos] = PLAYER;
	}

	s->posPlayer = tempPos;

	// Verifica se o Id é único e claro, qual seu valor.
	if ((*getStateId)(s)) {
		return 0;
	}

	addPath(c, s);

	// Retorna o efeito
	return c;
};

void placeThis(enum GridObject gridObject, int x, int y, State *s) {
	if (gridObject == EMPTY) {
		// Espaço vazio
		return;
	}

	// Como o mapa é considerado como um quadrado de 19 por 19, cada posição
	// pode ser linearizada como x+20*y. Assim, a posição (17,4) = 17 + 80 = 97
	int pos = x + 20 * y;

	// Colocamos o objeto no grid
	s->grid[pos] = gridObject;

	if (gridObject == PLAYER) {
		//É o player.
		s->posPlayer = pos;
		return;
	}
	if (gridObject == BOX) {
		//É uma caixa.
		s->posBoxes[s->boxes++] = pos;
		return;
	}

	int i = 0;
	while (s->posGoals[i] != 0) {
		++i;
	}

	if (gridObject == EMPTY_GOAL) {
		//É um alvo.
		s->posGoals[i] = pos;
		return;
	}

	if (gridObject == FILLED_GOAL) {
		//É um alvo e uma caixa.
		s->posGoals[i] = pos;
		s->posBoxes[s->boxes++] = pos;
		s->boxesOnGoals++;
		return;
	}

	if (gridObject == PLAYER_ON_GOAL) {
		//É o player e um alvo.
		s->posGoals[i] = pos;
		s->posPlayer = pos;
	}
}

// Função para construir o mapa
void buildMap(State *s, char *level) {

	// Número de caixas, global
	s->boxes = 0;

	// Inicializamos o número de caixas em objetivos
	s->boxesOnGoals = 0;

	// Inicializamos a posição das caixas
	memset(s->posBoxes, 0, 30);
	memset(s->posGoals, 0, 30);

	// Inicializamos o grid com espaços vazios
	memset(s->grid, EMPTY, 400);

	// Inicializamos a ação com nulo, pois é como saberemos que voltamos até o
	// começo do processo
	s->lastAction = malloc(sizeof(ActionList));
	s->lastAction->prevAction = NULL;
	s->lastAction->action = 0;

	// Abrimos o nível
	char str[16] = "levels/";
	strcat(str, level);
	FILE *file = fopen(str, "r");

	char line[20];

	int x, maxWidth, height;
	maxWidth = 0;
	height = -1;

	// Pegamos 20 caracteres de cada linha, até que a linha retorne nulo.
	while (fgets(line, 20, file) != NULL) {
		// Se a linha for 0 ou 10 (nova linha), estamos fora do mapa
		if (line[0] != 0 && line[0] != 10) {
			// Aumentamos a altura
			height++;

			// Pra cada caracter, colocamos o objeto no mapa.
			for (x = 0; (int)line[x] != 0 && (int)line[x] != 10; x++) {
				placeThis(line[x], x, height, s);
			}

			// Procuramos o maior x para chamar de largura
			if (x > maxWidth) {
				maxWidth = x;
			}

			x = 0;
		}
	}

	fclose(file);

	s->width = maxWidth;
	s->height = height;
};

// Função que verifica se o estado é final
// Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em
// mente, este não está preparado para níveis que possuam mais caixas que
// objetivos
unsigned char isFinal(State *s) {
	if (s->boxes == s->boxesOnGoals) {
		return 1;
	}
	return 0;
}
