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
	if (s->grid[pos] == '#') {
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
	int wallsUp = s->grid[pos - 20] == '#',
	    wallsDown = s->grid[pos + 20] == '#',
	    wallsLeft = s->grid[pos - 1] == '#',
	    wallsRight = s->grid[pos + 1] == '#';
	if (s->grid[pos] == '.' || s->grid[pos] == '*') {
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

	if (s->grid[tempPos] == '$' || s->grid[tempPos] == '*') {
		// Tem uma caixa na direção
		if (checkWallsAt(s, tempPos + movingParam) == 1 ||
		    s->grid[tempPos + movingParam] == '$' ||
		    s->grid[tempPos + movingParam] == '*') {
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
		if (s->grid[tempPos] == '*') {
			// A caixa estava sobre um alvo
			s->grid[tempPos] = '.';
			s->boxesOnGoals--;
		} else {
			// Não estava sobre nada
			s->grid[tempPos] = 32;
		}
		if (s->grid[tempPos + movingParam] == '.') {
			// A posição de destino da caixa é alvo
			s->grid[tempPos + movingParam] = '*';
			s->boxesOnGoals++;
		} else {
			s->grid[tempPos + movingParam] = '$';
		}
		// Move a caixa nas posições
		for (int i = 0; i < 30 && s->posBoxes[i] != 0; i++) {
			if (s->posBoxes[i] == tempPos) {
				s->posBoxes[i] += movingParam;
			}
		}
	}

	if (s->grid[s->posPlayer] == '+') {
		// Sokoban tava em cima de um alvo
		s->grid[s->posPlayer] = '.';
	} else {
		// Sokoban não estava sobre nada
		s->grid[s->posPlayer] = 32;
	}
	if (s->grid[tempPos] == '.') {
		// Sokoban está indo na direção de um alvo
		s->grid[tempPos] = '+';
	} else {
		// Não havia nada sobre a posição de destino
		s->grid[tempPos] = '@';
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