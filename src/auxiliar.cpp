#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <cerrno>
#include <sys/stat.h>

#include "constants.h"

using namespace std;

using Matrix = vector<vector<int>>;

bool criarDiretorioSaida(const char *dir) {
	int status = mkdir(dir, 0777);
	return status == 0 || errno == EEXIST;
}

bool validarNumeroInteiro(const string &text, int &value) {
	try {
		size_t endPos = 0;
		int number = stoi(text, &endPos);

		if (endPos != text.size()) {
			return false;
		}

		value = number;
		return true;
	} catch (...) {
		return false;
	}
}

Matrix gerarMatrizAleatoria(int rows, int cols) {
	Matrix matrix(rows, vector<int>(cols, 0));

    // o random_device é usado para obter uma semente aleatoria do sistema
	random_device rd;
    // o mt19937 é um gerador de numeros pseudo-aleatorios baseado no algoritmo Mersenne Twister
	mt19937 rng(rd());
    // o uniform_int_distribution é usado para gerar numeros inteiros aleatorios entre 0 e 9
	uniform_int_distribution<int> dist(0, 9);

	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			matrix[row][col] = dist(rng);
		}
	}

	return matrix;
}

bool criarArquivoMatriz(const string &filePath, const Matrix &matrix) {
    // o ofstream é usado para criar um fluxo de saida para escrever em arquivos
	ofstream novoArquivo(filePath);

	if (!novoArquivo.is_open()) {
		return false;
	}

	int linhas = static_cast<int>(matrix.size());
	int colunas = (linhas > 0) ? static_cast<int>(matrix[0].size()) : 0;

	novoArquivo << linhas << ' ' << colunas << '\n';

	for (int linha = 0; linha < linhas; ++linha) {
		for (int coluna = 0; coluna < colunas; ++coluna) {
			novoArquivo << matrix[linha][coluna];

			if (coluna + 1 < colunas) {
				novoArquivo << ' ';
			}
		}
		novoArquivo << '\n';
	}

	return true;
}

int main(int argc, char *argv[]) {
	if (argc != 5) {
        // cerr é usado para imprimir mensagens de erro no console
		cerr << "Uso: " << argv[0] << " n1 m1 n2 m2\n";
		cerr << "Exemplo: " << argv[0] << " 3 2 2 4\n";
		return 1;
	}

	int n1 = 0;
	int m1 = 0;
	int n2 = 0;
	int m2 = 0;

	bool okN1 = validarNumeroInteiro(argv[1], n1);
	bool okM1 = validarNumeroInteiro(argv[2], m1);
	bool okN2 = validarNumeroInteiro(argv[3], n2);
	bool okM2 = validarNumeroInteiro(argv[4], m2);

	if (!okN1 || !okM1 || !okN2 || !okM2) {
		cerr << "Erro: todos os argumentos devem ser inteiros validos.\n";
		return 1;
	}

	if (n1 <= 0 || m1 <= 0 || n2 <= 0 || m2 <= 0) {
		cerr << "Erro: as dimensoes devem ser maiores que zero.\n";
		return 1;
	}

	if (m1 != n2) {
		cerr << "Erro: matrizes incompativeis para multiplicacao (m1 deve ser igual a n2).\n";
		return 1;
	}

	Matrix matrix1 = gerarMatrizAleatoria(n1, m1);
	Matrix matrix2 = gerarMatrizAleatoria(n2, m2);

	if (!criarDiretorioSaida(AppPaths::dirSaida)) {
		cerr << "Erro: nao foi possivel criar o diretorio de saida.\n";
		return 1;
	}

	if (!criarDiretorioSaida(AppPaths::dirSaidaMatrizes)) {
		cerr << "Erro: nao foi possivel criar o diretorio de saida.\n";
		return 1;
	}

	string caminhoMatrizM1 = AppPaths::dirM1 + to_string(n1) + "x" + to_string(m1) + ".txt";
	string caminhoMatrizM2 = AppPaths::dirM2 + to_string(n2) + "x" + to_string(m2) + ".txt";

	bool savedM1 = criarArquivoMatriz(caminhoMatrizM1, matrix1);
	bool savedM2 = criarArquivoMatriz(caminhoMatrizM2, matrix2);

	if (!savedM1 || !savedM2) {
		cerr << "Erro: nao foi possivel salvar uma ou ambas as matrizes nos arquivos de saida.\n";
		return 1;
	}

	cout << "Arquivos gerados com sucesso:\n";
	cout << "- " << caminhoMatrizM1 << " (" << n1 << "x" << m1 << ")\n";
	cout << "- " << caminhoMatrizM2 << " (" << n2 << "x" << m2 << ")\n";

	return 0;
}
