OUTPUT_DIR = target

all: $(OUTPUT_DIR)/SokobanMP_Andar.exe $(OUTPUT_DIR)/SokobanMP_Digito.exe \
$(OUTPUT_DIR)/Sokoban_HashTableList.exe $(OUTPUT_DIR)/Sokoban_Trie.exe \
$(OUTPUT_DIR)/Sokoban_Heuristic.exe

$(OUTPUT_DIR)/sort.o: $(OUTPUT_DIR) common/sort.h common/sort.c
	gcc -O2 -o $(OUTPUT_DIR)/sort.o -c common/sort.c

$(OUTPUT_DIR)/common.o: $(OUTPUT_DIR) common/common.h common/structures.h common/common.c
	gcc -O2 -o $(OUTPUT_DIR)/common.o -c common/common.c

$(OUTPUT_DIR)/hash-table.o: $(OUTPUT_DIR) hash-table/hash-table.h common/structures.h hash-table/hash-table.c
	gcc -O2 -o $(OUTPUT_DIR)/hash-table.o -c hash-table/hash-table.c

$(OUTPUT_DIR)/trie.o: $(OUTPUT_DIR) trie/trie.h common/structures.h common/sort.h common/common.h trie/trie.c
	gcc -O2 -o $(OUTPUT_DIR)/trie.o -c trie/trie.c

$(OUTPUT_DIR)/SokobanMP_Andar.exe: trie/SokobanMP_Andar.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o
	gcc -O2 -o target/SokobanMP_Andar.exe -fopenmp trie/SokobanMP_Andar.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o

$(OUTPUT_DIR)/SokobanMP_Digito.exe: trie/SokobanMP_Digito.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o
	gcc -O2 -o target/SokobanMP_Digito.exe -fopenmp trie/SokobanMP_Digito.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o

$(OUTPUT_DIR)/Sokoban_Trie.exe: trie/Sokoban_Trie.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o
	gcc -O2 -o target/Sokoban_Trie.exe trie/Sokoban_Trie.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/trie.o

$(OUTPUT_DIR)/Sokoban_HashTableList.exe: hash-table/Sokoban_HashTableList.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/hash-table.o
	gcc -O2 -o target/Sokoban_HashTableList.exe hash-table/Sokoban_HashTableList.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/hash-table.o
$(OUTPUT_DIR)/Sokoban_Heuristic.exe: hash-table/Sokoban_Heuristic.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/hash-table.o
	gcc -O2 -o target/Sokoban_Heuristic.exe hash-table/Sokoban_Heuristic.c $(OUTPUT_DIR)/sort.o $(OUTPUT_DIR)/common.o $(OUTPUT_DIR)/hash-table.o

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

test:
	$(OUTPUT_DIR)/Sokoban_HashTableList.exe level_-1
	$(OUTPUT_DIR)/Sokoban_Trie.exe level_00
	$(OUTPUT_DIR)/Sokoban_Heuristic.exe level_-1
	$(OUTPUT_DIR)/SokobanMP_Andar.exe level_-1
	$(OUTPUT_DIR)/SokobanMP_Digito.exe level_-1

clean:
	rm -rf target/