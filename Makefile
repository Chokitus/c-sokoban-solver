OUTPUT_DIR = target

all: $(OUTPUT_DIR)/SokobanMP_Andar.exe $(OUTPUT_DIR)/SokobanMP_Digito.exe \
$(OUTPUT_DIR)/Sokoban_HashTableList.exe $(OUTPUT_DIR)/Sokoban_Trie.exe \
$(OUTPUT_DIR)/Sokoban_Heuristic.exe

$(OUTPUT_DIR)/sort.o: $(OUTPUT_DIR) common/sort.h common/sort.c
	gcc -O2 -o $(OUTPUT_DIR)/sort.o -c common/sort.c

$(OUTPUT_DIR)/SokobanMP_Andar.exe: brute-force/multi-thread/SokobanMP_Andar.c $(OUTPUT_DIR)/sort.o
	gcc -O2 -o target/SokobanMP_Andar.exe -fopenmp brute-force/multi-thread/SokobanMP_Andar.c $(OUTPUT_DIR)/*.o

$(OUTPUT_DIR)/SokobanMP_Digito.exe: brute-force/multi-thread/SokobanMP_Digito.c $(OUTPUT_DIR)/sort.o
	gcc -O2 -o target/SokobanMP_Digito.exe -fopenmp brute-force/multi-thread/SokobanMP_Digito.c $(OUTPUT_DIR)/*.o

$(OUTPUT_DIR)/Sokoban_HashTableList.exe: brute-force/single-thread/Sokoban_HashTableList.c $(OUTPUT_DIR)/sort.o
	gcc -O2 -o target/Sokoban_HashTableList.exe brute-force/single-thread/Sokoban_HashTableList.c $(OUTPUT_DIR)/*.o

$(OUTPUT_DIR)/Sokoban_Trie.exe: brute-force/single-thread/Sokoban_Trie.c $(OUTPUT_DIR)/sort.o
	gcc -O2 -o target/Sokoban_Trie.exe brute-force/single-thread/Sokoban_Trie.c $(OUTPUT_DIR)/*.o

$(OUTPUT_DIR)/Sokoban_Heuristic.exe: heuristic/sokoban_heuristic.c $(OUTPUT_DIR)/sort.o
	gcc -O2 -o target/Sokoban_Heuristic.exe heuristic/sokoban_heuristic.c $(OUTPUT_DIR)/*.o

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

clean:
	rm -rf target/