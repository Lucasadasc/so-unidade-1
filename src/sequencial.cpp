#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <direct.h>

#include "constants.h"

using namespace std;

using Matrix = vector<vector<int>>;

bool carregarArquivoMatriz(const string &caminhoArquivo, Matrix &matriz) {
	ifstream arquivoMatriz(caminhoArquivo);
	if (!arquivoMatriz.is_open()) {
		return false;
	}

	int rows = 0;
	int cols = 0;
	if (!(arquivoMatriz >> rows >> cols) || rows <= 0 || cols <= 0) {
		return false;
	}

    // a função assign é usada para redimensionar a matriz e preencher com zeros
	matriz.assign(rows, vector<int>(cols, 0));
	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			if (!(arquivoMatriz >> matriz[row][col])) {
				return false;
			}
		}
	}

	return true;
}

Matrix multiplicarSequencialmente(const Matrix &m1, const Matrix &m2) {
	int rows = static_cast<int>(m1.size());
	int common = static_cast<int>(m1[0].size());
	int cols = static_cast<int>(m2[0].size());

	Matrix result(rows, vector<int>(cols, 0));
	for (int row = 0; row < rows; ++row) {
		for (int col = 0; col < cols; ++col) {
			int sum = 0;
			for (int k = 0; k < common; ++k) {
				sum += m1[row][k] * m2[k][col];
			}
			result[row][col] = sum;
		}
	}

	return result;
}

bool salvarResultadoComTempo(const string &caminhoArquivo, const Matrix &matriz, double tempoExecucaoS) {
	ofstream novoArquivo(caminhoArquivo);
	if (!novoArquivo.is_open()) {
		return false;
	}

	int linhas = static_cast<int>(matriz.size());
	int colunas = linhas > 0 ? static_cast<int>(matriz[0].size()) : 0;
	novoArquivo << linhas << ' ' << colunas << '\n';

	for (int row = 0; row < linhas; ++row) {
		for (int col = 0; col < colunas; ++col) {
			novoArquivo << "c" << (row + 1) << (col + 1) << ' ' << matriz[row][col] << '\n';
		}
	}

	novoArquivo << tempoExecucaoS << '\n';
	return true;
}

int main(int argc, char *argv[]) {
	string caminhoResultado = AppPaths::dirResultadoSequencial;

	if (argc != 3) {
		cerr << "Passe os seguintes argumentos: matriz_m1.txt matriz_m2.txt\n";
		return 1;
	}

	string caminhoMatriz1 = argv[1];
	string caminhoMatriz2 = argv[2];

	Matrix m1;
	Matrix m2;
	if (!carregarArquivoMatriz(caminhoMatriz1, m1) || !carregarArquivoMatriz(caminhoMatriz2, m2)) {
		cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		cerr << "M1: " << caminhoMatriz1 << "\n";
		cerr << "M2: " << caminhoMatriz2 << "\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	_mkdir(AppPaths::dirSaidaMatrizes);

	auto start = chrono::high_resolution_clock::now();
	Matrix result = multiplicarSequencialmente(m1, m2);
	auto end = chrono::high_resolution_clock::now();

	double elapsedS = chrono::duration<double>(end - start).count();
	if (!salvarResultadoComTempo(caminhoResultado, result, elapsedS)) {
		cerr << "Erro: nao foi possivel salvar o resultado em arquivo.\n";
		return 1;
	}

	cout << fixed << setprecision(6);
	cout << "Tempo de multiplicacao: " << elapsedS << " s\n";
	
	return 0;
}
