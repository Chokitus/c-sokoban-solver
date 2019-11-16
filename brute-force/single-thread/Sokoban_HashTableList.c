#include "../../common/sort.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define copyState(s, s2) memcpy(s2, s, sizeof(State))

//Valor do módulo usado no hashing do id (que por si só é um hash)
#define ID_HASH 100000000

#define NANOS ((unsigned long)1000000000)

//Tamanho máximo do caminho
#define MAX_PATH_SIZE 3000

enum Direcoes{
	direita = 0,
	esquerda = 1,
	cima = 2,
	baixo = 3
};

//Estado que estaremos analisando
typedef struct State{
	//Todas as posições se baseiam no fato que não há mapa com mais de 19 de largura/altura. Portanto, a posição é 20*y+x;
	unsigned short posPlayer;
	unsigned short posBoxes[30];
	unsigned short posGoals[30];
	unsigned char grid[400];
	//Quantidade de caixas encontradas
	unsigned char boxes;
	//Quantidade de caixas no objetivo
	unsigned char boxesOnGoals;
	//Caminho para a solução
	unsigned char path[MAX_PATH_SIZE];
	unsigned int pathSize;
	unsigned int heuristic;
} State;

//Lista ligada (com hash de resto da divisão) para os ids visitados.
typedef struct visitedId{
	unsigned char *stateId;
	struct visitedId* nextId;
} visitedId;

//Lista ligada com cada estado.
typedef struct Node{
	State *state;
	struct Node *nextState;
} Node;

//Quantidade de nós visitados
unsigned int numberOfNodes;

//Array para o espalhamento dos IDs.
visitedId *idList[ID_HASH];

//Ponteiro para o último nó
Node **last;

void printPath(State *s){
	printf("%s\n", s->path);
}

//Função que procura o id na lista
unsigned char findId(char *idHash, unsigned long long int id){
	//Primeiro pegamos o index
	unsigned int idIndex = id % ID_HASH;

	//Começamos da cabeça da lista
	visitedId **idIterator = &(idList[idIndex]);

	//E iteramos até o final
	while((*idIterator) != NULL){
		if(strcmp((*idIterator)->stateId, idHash) == 0){
			//Encontrou o id, significa que este estado foi visitado antes
			return 1;
		}
		idIterator = &((*idIterator)->nextId);
	}
	//Não encontrou, é um estado novo
	return 0;
}

void insertId(unsigned char *idHash, unsigned long long int id){
	//Começamos da cabeça da lista
	visitedId **idIterator = &(idList[id % ID_HASH]);

	//Se o primeiro id é vazio, devemos criar um novo id
	if((*idIterator) == NULL){
		*idIterator = (visitedId*) malloc(sizeof(visitedId));
		(*idIterator)->stateId = idHash;
		(*idIterator)->nextId = NULL;
		return;
	}
	//O primeiro não era vazio, então iteramos até encontrar o último
	while((*idIterator)->nextId != NULL){
		idIterator = &((*idIterator)->nextId);
	}
	//Criamos um id
	(*idIterator)->nextId = (visitedId*) malloc(sizeof(visitedId));
	(*idIterator)->nextId->stateId = idHash;
	(*idIterator)->nextId->nextId = NULL;
}

//-------------------------------------------------------------------

//Função que retorna o índice relacionado ao ID.
//Esta é a função de hash djb2:
unsigned long long int getIdIndex(State *s){
	unsigned long long h = 5381;

	for(int i = 0; i < s->boxes; i++){
		h = ((h << 5) + h) + s->posBoxes[i]/2 + s->posBoxes[i] % 20;
	}
	h = ((h << 5) + h) + s->posPlayer/2 + s->posPlayer % 20;
}

//Função de Hash para pegar o ID do Estado
unsigned char getStateId(State *s){

	//Fazemos um sort pois a ordem das caixas não pode importar
	quickSort(s->posBoxes, 0, s->boxes - 1);

	//Pensando que cada caixa tem até 3 dígitos, precisamos de 3+1 dígitos para cada baixa,
	//e após 3 dígitos para o player
	unsigned char idHash[s->boxes*4+4];

	//Criamos um buffer
	unsigned char buffer[8];

	memset(idHash, 0, s->boxes*4+4);

	unsigned long long h = getIdIndex(s);

	//Para cada caixa, colocamos sua posição no buffer, e concatenamos no idHash
	for(int i = 0; i < s->boxes; i++){
		sprintf(buffer, "%d", s->posBoxes[i]);
		strcat(idHash, buffer);
		strcat(idHash, " ");
	}

	//Fazemos o mesmo para a posição do player
	sprintf(buffer, "%d", s->posPlayer);
	strcat(idHash, buffer);

	//Procuramos o ID na lista. Se estiver, retornamos verdadeiro
	if(findId(idHash, h) == 1){
		return 1;
	}

	//Sendo id único, inserimos o mesmo
	insertId(idHash, h);

	return 0;
};

//Adicionamos o movimento feito na lista de movimentos
void addPath(char c, State *s){
	//Se atingimos o limite de tamanho, encerramos
	if(s->pathSize == MAX_PATH_SIZE){
		return;
	}
	s->path[s->pathSize++] = c;
};

//
void placeThis(char c, int x, int y, struct State *s){
	if(c == 32){
		// Espaço vazio
		return;
	}

	//Como o mapa é considerado como um quadrado de 19 por 19, cada posição pode ser linearizada
	//como x+20*y. Assim, a posição (17,4) = 17 + 80 = 97
	int pos = x + 20*y;

	//Colocamos o objeto no grid
	s->grid[pos] = c;

	if(c == '@'){
		//É o player.
		s->posPlayer = pos;
		return;
	}
	if(c == '$'){
		//É uma caixa.
		s->posBoxes[s->boxes++] = pos;
		return;
	}

	int i = 0;
	while(s->posGoals[i] != 0){
		++i;
	}

	if(c == '.'){
		//É um alvo.
		s->posGoals[i] = pos;
		return;
	}

	if(c == '*'){
		//É um alvo e uma caixa.
		s->posGoals[i] = pos;
		s->posBoxes[s->boxes++] = pos;
		s->boxesOnGoals++;
		return;
	}

	if(c == '+'){
		//É o player e um alvo.
		s->posGoals[i] = pos;
		s->posPlayer = pos;
	}
}

//Função para construir o mapa
void buildMap(struct State *s, char *level){

	//Inicializamos algumas das variáveis de State
	s->boxes = 0;
	s->boxesOnGoals = 0;
	s->pathSize = 0;
	memset(s->posGoals, 0, 30);
	memset(s->posBoxes, 0, 30);
	memset(s->path, 0, 200);

	//Apontamos cada um dos Ids para nulo.
	memset(idList, 0, ID_HASH);

	//Inicializamos o ponteiro para o último estado
	last = (Node**) malloc(sizeof(void*));
	(*last) = NULL;

	//Abrimos o nível
	char str[16] = "levels/";
	strcat(str, level);
	FILE *file = fopen(str,"r");

	char line[20];

	int x, maxWidth, height;
	maxWidth = 0;
	height = -1;

	//Pegamos 20 caracteres de cada linha, até que a linha retorne nulo.
	while(fgets(line, 20, file) != NULL){
		//Se a linha for 0 ou 10 (nova linha), estamos fora do mapa
		if(line[0] != 0 && line[0] != 10){
			//Aumentamos a altura
			height++;

			//Pra cada caracter, colocamos o objeto no mapa.
			for(x = 0; (int) line[x] != 0 && (int) line[x] != 10; x++){
				placeThis(line[x], x, height, s);
			}

			//Procuramos o maior x para chamar de largura
			if(x > maxWidth){
				maxWidth = x;
			}

			x = 0;
		}
	}

	fclose(file);

	getStateId(s);
};

//Função simples que retorna se a posição pos é uma parede
int checkWallsAt(State *s, unsigned int pos){
	if(s->grid[pos] == '#'){
		return 1;
	}
	return 0;
};

//Função que verifica se prendemos a caixa
/*
	 #
	#$
	Procuramos "cantos" como este. Se a caixa está num canto, e este canto não é um objetivo, travamos a caixa
	Esta é uma posição exemplo que não seria explorada:
	#######
	#   @$# <- Note que a caixa não poderá ser movida
	#     #
	#. #  #
	#. $  #
	#.$$  #
	#.#  @#
	#######
*/
unsigned char boxTrapped(State *s, int pos){
    int wallsUp = s->grid[pos-20] == '#', wallsDown = s->grid[pos+20] == '#', wallsLeft = s->grid[pos-1] == '#', wallsRight = s->grid[pos+1] == '#';
    if(s->grid[pos] == '.'){
        	return 0;
    }
    if(wallsUp && wallsLeft){
        	return 1;
    }
    if(wallsUp && wallsRight){
        	return 1;
    }
    if(wallsDown && wallsLeft){
      	return 1;
    }
    if(wallsDown && wallsRight){
      	return 1;
    }

    return 0;
}

//Função que usaremos intensivamente para mover o sokoban
char movePlayer(struct State *s, int dir){
	//dir == 0 -> direita
	//dir == 1 -> esquerda
	//dir == 2 -> cima
	//dir == 3 -> baixo
	char c;
	unsigned char box = 0;
	int tempPos, i = -1;
	int movingParam;
	switch(dir){
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

	//Usamos o trecho de código para obter o parametro de movimento, assim como o caracter.
	//Como parâmetro de movimento, queremos transformar a visao 1D do array no movimento 2D do personagem, ou seja,
	//dado que cada linha possui 20 colunas, "andar para cima" recua 20 colunas, por isso o movingParam é -20.
	//A mesma lógica se aplica para os outros

	tempPos = s->posPlayer + movingParam;

	if(checkWallsAt(s, tempPos)){
		//Tem uma parede.
		return 0;
	}

	if(s->grid[tempPos] == '$' || s->grid[tempPos] == '*'){
		//Tem uma caixa na direção
		if(checkWallsAt(s, tempPos + movingParam) == 1 ||
				s->grid[tempPos + movingParam] == '$' ||
				s->grid[tempPos + movingParam] == '*'){
			//Tem uma parede ou caixa após a caixa.
			return 0;
		};
		//Deixa a letra maiuscula
		c -= 32;
		box = 1;
	}

	//Efetiva o movimento
	if(box != 0){
		if(boxTrapped(s, tempPos + movingParam)){
            return 0;
        }
		//Empurrou uma caixa
		if(s->grid[tempPos] == '*'){
			//A caixa estava sobre um alvo
			s->grid[tempPos] = '.';
			s->boxesOnGoals--;
		}
		else{
			//Não estava sobre nada
			s->grid[tempPos] = 32;
		}
		if(s->grid[tempPos + movingParam] == '.'){
			//A posição de destino da caixa é alvo
			s->grid[tempPos + movingParam] = '*';
			s->boxesOnGoals++;
		}
		else{
			s->grid[tempPos + movingParam] = '$';
		}
		//Move a caixa nas posições
		for(int i = 0; i < 30 && s->posBoxes[i] != 0; i++){
			if(s->posBoxes[i] == tempPos){
				s->posBoxes[i] += movingParam;
			}
		}
	}

	s->posPlayer = tempPos;

	//Verifica se o Id é único e claro, qual seu valor.
	if(getStateId(s)){
		return 0;
	}

	addPath(c, s);

	//Retorna o efeito
	return c;
};

//Função que verifica se o estado é final
//Dado que este algoritmo foi implementado possuindo os nívels -1, 00 e 01 em mente,
//este não está preparado para níveis que possuam mais caixas que objetivos
unsigned char isFinal(State *s){
	if(s->boxes == s->boxesOnGoals){
		return 1;
	}
	return 0;
}

//Função que usamos para inserir o estado
unsigned char insertState(Node **root, State *s){
	if(isFinal(s)){
		//É final
		return 1;
	}

	//Lista está vazia.
	if((*root) == NULL){
		//Criamos o nó
		(*root) = (Node*) malloc(sizeof(Node));

		//Last também estará nulo neste caso, portanto criaremos um novo.
		(*last) = (Node*) malloc(sizeof(Node));
		(*last)->state = NULL;

		//A raiz aponta para o último
		(*root)->nextState = (*last);

		//Colocamos o estado no nó raiz
		(*root)->state = (State*) malloc(sizeof(State));

		copyState(s, (*root)->state);
		return 0;
	}

	//Colocamos este estado no último
	(*last)->state = (State*) malloc(sizeof(State));
	copyState(s, (*last)->state);
	(*last)->nextState = (Node*) malloc(sizeof(Node));
	(*last)->nextState->state = NULL;

	//Mudamos a posição do último estado.
	last = &((*last)->nextState);
	return 0;
}

//Função que devolve, em rootState, o primeiro estado na lista cuja raiz é root
void popState(Node **root, State** rootState){

	//Transfere o estado
	*rootState = (*root)->state;

	//Se o próximo esta vazio, este deve ser anulado
	if((*root)->nextState == NULL){
		free((*root));
		(*root) = NULL;
		return;
	}

	//Há mais de um, portanto devemos trocar de lugar o antigo com o novo
	Node *tempNode = (*root);
	(*root) = (*root)->nextState;
	free(tempNode);
}

int main(int argc, char* argv[]){
	struct timespec before, after;
   time_t nSeconds;

   //Começamos a contagem de tempo.
   clock_gettime(CLOCK_REALTIME, &before);

	//Criamos a raiz
	Node *root = (Node*) malloc(sizeof(Node));

	State *s, *rootState;
	unsigned char final;

	//Alocamos espaço para a raiz
	root->state = (State*) malloc(sizeof(State));
	root->nextState = NULL;

	//Criamos o mapa
	buildMap(root->state, argv[1]);

	//Alocamos espaço para o estado que estará sujeito à ser movido
	s = (State*) malloc(sizeof(State));

	while(final != 1){
		//Pegamos o primeiro estado em root e devolvemos em rootState
		popState(&root, &rootState);

		//Cada cada direção
		for(int i = 0; i < 4; i++){

			//Copiamos o estado para s
			copyState(rootState, s);

			//Se foi possível mover, insere o estado
			if(movePlayer(s, i) != 0){

				//Verifica se este estado é final, junto com inseri-lo na lista
				final = insertState(&root, s);
			}

			//Se for final, printamos o caminho
			if(final == 1){
            printPath(s);
				break;
			}
		}
		//Limpamos o conteudo em rootState
		free(rootState);
	}
	//Finalizamos a contagem de tempo.
	clock_gettime(CLOCK_REALTIME, &after);

	//Calcula o tempo passado em microssegundos.
	nSeconds = after.tv_nsec - before.tv_nsec + (after.tv_sec - before.tv_sec) * NANOS;

	printf("Tempo total: %lu ns - %lf s\n",  nSeconds, (double)nSeconds/NANOS);

	return 0;
}