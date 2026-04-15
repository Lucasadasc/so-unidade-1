# so-unidade-1

Compilar auxiliar:
g++ -Wall -Wextra -g3 src/auxiliar.cpp -o output/auxiliar

Compilar sequencial:
g++ -Wall -Wextra -g3 src/sequencial.cpp -o output/sequencial

Compilar paralelo com threads:
g++ -Wall -Wextra -g3 src/paraleloThreads.cpp -o output/paraleloThreads -pthread

Compilar paralelo com processos:
g++ -Wall -Wextra -g3 src/paraleloProcessos.cpp -o output/paraleloProcessos -pthread

## Execucao no Linux

Gerar matrizes:
./output/auxiliar 100 100 100 100

Rodar sequencial:
./output/sequencial output/matrizes/matriz_m1100x100.txt output/matrizes/matriz_m2100x100.txt

Rodar threads:
./output/paraleloThreads output/matrizes/matriz_m1100x100.txt output/matrizes/matriz_m2100x100.txt 2

Rodar processos:
./output/paraleloProcessos output/matrizes/matriz_m1100x100.txt output/matrizes/matriz_m2100x100.txt 2

## Exemplo de loop no Bash

Execute a partir da raiz do projeto:

```bash
	for i in $(seq 1 10); do
		./output/sequencial output/matrizes/matriz_m1100x100.txt output/matrizes/matriz_m2100x100.txt
	done
```
