#include "../common/sort.h"
#include "../common/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_Node 100000000
#define ID_HASH 100000000
#define MAX_PATH_SIZE 3000

// Estado que estaremos analisando
typedef struct State {
	// Todas as posições se baseiam no fato que não há mapa com mais de 19 de
	// largura/altura. Portanto, a posição é 20*y+x;
	unsigned char posPlayer;
	unsigned short posBoxes[30];
	unsigned short posGoals[30];
	unsigned char grid[400];
	// Quantidade de caixas encontradas
	unsigned char boxes;
	// Quantidade de caixas no objetivo
	unsigned char boxesOnGoals;
	// Caminho para a solução
	unsigned char path[MAX_PATH_SIZE];
	unsigned int pathSize;
	unsigned int heuristic;
} State;

// Lista ligada (com hash de resto da divisão) para os ids visitados.
typedef struct visitedId {
	unsigned char *stateId;
	struct visitedId *nextId;
} visitedId;

// Lista ligada (ordenada) com cada estado.
typedef struct Node {
	State *state;
	struct Node *nextState;
} Node;

// Quantidade de nós visitados
unsigned int numberOfNodes;

// Array para o espalhamento dos IDs.
visitedId *idList[ID_HASH];

unsigned int activeStates = 0;
unsigned int storedIds = 0;
unsigned int filteredIds = 0;
unsigned int movesBlocked = 0;

Node **last;

void printPath(State *s) {
	int i = 0;
	printf("Caminho: %s\n", s->path);
}

// Função que procura o id na lista
unsigned char findId(char *idHash, unsigned long long int id) {
	// Primeiro pegamos o index
	unsigned int idIndex = id % ID_HASH;

	// Começamos da cabeça da lista
	visitedId **idIterator = &(idList[idIndex]);

	// E iteramos até o final
	while ((*idIterator) != NULL) {
		if (strcmp((*idIterator)->stateId, idHash) == 0) {
			// Encontrou o id, significa que este estado foi visitado antes
			return 1;
		}
		idIterator = &((*idIterator)->nextId);
	}
	// Não encontrou, é um estado novo
	return 0;
}

void insertId(unsigned char *idHash, unsigned long long int id) {
	// Pegamos o índice
	storedIds++;

	// Começamos da cabeça da lista
	visitedId **idIterator = &(idList[id % ID_HASH]);

	if ((*idIterator) == NULL) {
		*idIterator = (visitedId *)malloc(sizeof(visitedId));
		(*idIterator)->stateId = idHash;
		(*idIterator)->nextId = NULL;
		return;
	}
	while ((*idIterator)->nextId != NULL) {
		idIterator = &((*idIterator)->nextId);
	}
	(*idIterator)->nextId = (visitedId *)malloc(sizeof(visitedId));
	(*idIterator)->nextId->stateId = idHash;
	(*idIterator)->nextId->nextId = NULL;
}

//-------------------------------------------------------------------

unsigned long long int getIdIndex(State *s) {
	unsigned long long h = 5381;

	for (int i = 0; i < s->boxes; i++) {
		h = ((h << 5) + h) + s->posBoxes[i] / 2 + s->posBoxes[i] % 20;
	}
	h = ((h << 5) + h) + s->posPlayer / 2 + s->posPlayer % 20;
}

// Função de Hash para pegar o ID do Estado
unsigned char getStateId(State *s) {
	// Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);

	unsigned char idHash[s->boxes * 4 + 5];
	unsigned char buffer[5];
	memset(idHash, 0, s->boxes * 5 + 5);

	unsigned long long h = getIdIndex(s);
	for (int i = 0; i < s->boxes; i++) {
		sprintf(buffer, "%hu", s->posBoxes[i]);
		strcat(idHash, buffer);
		strcat(idHash, " ");
	}
	sprintf(buffer, "%d", s->posPlayer);
	strcat(idHash, buffer);

	// Procuramos o ID na lista. Se estiver, retornamos verdadeiro
	if (findId(idHash, h) == 1) {
		filteredIds++;
		return 1;
	}

	// Sendo id único, inserimos o mesmo
	insertId(idHash, h);

	return 0;
}

void addPath(char c, State *s) {
	// Se atingimos o limite de tamanho, encerramos
	if (s->pathSize == MAX_PATH_SIZE) {
		return;
	}
	s->path[s->pathSize++] = c;
};

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
			//seria a raiz do mesmo, mas isto não importa para nós
			dist = (x - xGoal) * (x - xGoal) + (y - yGoal) * (y - yGoal);

			if (dist < min) {
				// Devemos tomar a mínima das distâncias, pois níveis com goals
				// muito separados nos dariam problemas
				if (dist == 0) {
					// Se min == 0, então a caixa está em um goal, então é
					// inútil procurar nos demais.
					break;
				}
				if (s->grid[s->posGoals[j]] != '*') {
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

	// s->heuristic = h;

	// Dividimos a mesma pela quantidade de caixas nos alvos para que esta ação
	// seja recompensada
	s->heuristic = h / (s->boxesOnGoals + 1);
	// s->heuristic = 1;
	return h;
};

void placeThis(char c, int x, int y, struct State *s) {
	if ((int)c == 32) {
		// Espaço vazio
		return;
	}

	int pos = x + 20 * y;

	s->grid[pos] = c;

	if (c == '@') {
		//É o player.
		s->posPlayer = pos;
		return;
	}
	if (c == '$') {
		//É uma caixa.
		s->posBoxes[s->boxes++] = pos;
		return;
	}

	int i = 0;
	while (s->posGoals[i] != 0) {
		++i;
	}

	if (c == '.') {
		//É um alvo.
		s->posGoals[i] = pos;
		return;
	}

	if (c == '*') {
		//É um alvo e uma caixa.
		s->posGoals[i] = pos;
		s->posBoxes[s->boxes++] = pos;
		s->boxesOnGoals++;
		return;
	}

	if (c == '+') {
		//É o player e um alvo.
		s->posGoals[i] = pos;
		s->posPlayer = pos;
	}
}

void buildMap(struct State *s, char *level) {
	s->boxes = 0;
	s->boxesOnGoals = 0;
	s->pathSize = 0;

	memset(s->posGoals, 0, 30);
	memset(s->posBoxes, 0, 30);
	memset(s->path, 0, 200);
	memset(idList, 0, ID_HASH);

	last = (Node **)malloc(sizeof(void *));
	(*last) = NULL;

	// Abrimos o níve l
	char str[16] = "levels/";

	strcat(str, level);
	// FILE *file = fopen("levels/level_00","r");
	FILE *file = fopen(str, "r");

	char line[20];

	int x, maxWidth, height;
	maxWidth = 0;
	height = -1;

	while (fgets(line, 20, file) != NULL) {
		if (line[0] != 0 && line[0] != 10) {

			height++;

			for (x = 0; (int)line[x] != 0 && (int)line[x] != 10; x++) {
				printf("%c ", line[x]);
				placeThis(line[x], x, height, s);
			}
			printf(" %d %d\n", height * 20, 0);
			if (x > maxWidth) {
				maxWidth = x;
			}
			x = 0;
		}
	}

	fclose(file);

	getHeuristic(s);
	getStateId(s);

	printf("\nO mapa é %d x %d\n", maxWidth, height + 1);
	printf("O mapa possui %d caixas.\n", s->boxes);
};

int checkWallsAt(State *s, unsigned int pos) {
	if (s->grid[pos] == '#') {
		return 1;
	}
	return 0;
};

int sign(int x) { return (x > 0) - (x < 0); }

unsigned char boxTrapped(State *s, int pos) {
	int wallsUp = s->grid[pos - 20] == '#',
	    wallsDown = s->grid[pos + 20] == '#',
	    wallsLeft = s->grid[pos - 1] == '#',
	    wallsRight = s->grid[pos + 1] == '#';
	if (s->grid[pos] == '.') {
		return 0;
	}
	if (wallsUp && wallsLeft) {
		movesBlocked++;
		return 1;
	}
	if (wallsUp && wallsRight) {
		movesBlocked++;
		return 1;
	}
	if (wallsDown && wallsLeft) {
		movesBlocked++;
		return 1;
	}
	if (wallsDown && wallsRight) {
		movesBlocked++;
		return 1;
	}

	return 0;
}

char movePlayer(struct State *s, int dir) {
	// dir == 0 -> direita
	// dir == 1 -> esquerda
	// dir == 2 -> cima
	// dir == 3 -> baixo
	char c;
	unsigned char box = 0;
	int tempPos, i = -1;
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

	s->posPlayer = tempPos;

	// Verifica se o Id é único e claro, qual seu valor.
	if (getStateId(s)) {
		return 0;
	}

	addPath(c, s);

	// Retorna o efeito
	return c;
};

unsigned char isFinal(State *s) {
	if (s->boxes == s->boxesOnGoals) {
		return 1;
	}
	return 0;
}

unsigned char insertState(Node **root, State *s) {
	if (isFinal(s)) {
		//É final
		return 1;
	}

	numberOfNodes++;
	activeStates++;

	// Lista está vazia.
	if ((*root) == NULL) {
		// Criamos o nó
		(*root) = (Node *)malloc(sizeof(Node));

		// Last também estará nulo neste caso, portanto criaremos um novo.
		(*last) = (Node *)malloc(sizeof(Node));
		(*last)->state = NULL;

		(*root)->nextState = (*last);
		numberOfNodes++;
		// Colocamos o estado no nó
		(*root)->state = (State *)malloc(sizeof(State));
		copyState(s, (*root)->state);
		return 0;
	}

	// Colocamos este estado no último
	(*last)->state = (State *)malloc(sizeof(State));
	copyState(s, (*last)->state);
	(*last)->nextState = (Node *)malloc(sizeof(Node));
	(*last)->nextState->state = NULL;

	// Mudamos a posição do último estado.
	last = &((*last)->nextState);
	return 0;
}

void popState(Node **s, State **rootState) {
	*rootState = (*s)->state;

	activeStates--;

	// Se o próximo esta vazio, este deve ser anulado
	if ((*s)->nextState == NULL) {
		free((*s));
		(*s) = NULL;
		return;
	}

	// Há mais de um, portanto devemos trocar de lugar o antigo com o novo
	Node *tempNode = (*s);
	(*s) = (*s)->nextState;
	free(tempNode);
}

int main(int argc, char *argv[]) {
	Node *root = (Node *)malloc(sizeof(Node));
	State *s, *rootState;
	unsigned char final;

	root->state = (State *)malloc(sizeof(State));
	root->nextState = NULL;

	numberOfNodes = 1;
	activeStates = 1;

	buildMap(root->state, argv[1]);

	s = (State *)malloc(sizeof(State));

	while (numberOfNodes < MAX_Node && final != 1) {
		if (numberOfNodes % 10000 == 0) {
			printf("%d, %d estados ocupando %ld MB, %d ids ocupando %ld MB\n",
			       numberOfNodes, activeStates,
			       activeStates * (sizeof(Node) + sizeof(State)) / 1048576,
			       storedIds, storedIds * sizeof(visitedId) / 1048576);
			printf("%d ids filtrados\n", filteredIds);
			printf("%d moveBlocks\n\n", movesBlocked);
		}

		free(rootState);

		popState(&root, &rootState);

		for (int i = 0; i < 4; i++) {
			copyState(rootState, s);
			if (movePlayer(s, i) != 0) {
				final = insertState(&root, s);
			}

			if (final == 1) {
				printf("Achei a solução!\n");
				printPath(s);
				break;
			}
		}
	}

	return 0;
}
