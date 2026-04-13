#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <direct.h>

#include "constants.h"

using Matrix = std::vector<std::vector<int>>;

bool carregarArquivoMatriz(const std::string &caminhoArquivo, Matrix &matriz) {
	std::ifstream arquivoMatriz(caminhoArquivo);
	if (!arquivoMatriz.is_open()) {
		return false;
	}

	int rows = 0;
	int cols = 0;
	if (!(arquivoMatriz >> rows >> cols) || rows <= 0 || cols <= 0) {
		return false;
	}

    // a função assign é usada para redimensionar a matriz e preencher com zeros
	matriz.assign(rows, std::vector<int>(cols, 0));
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

	Matrix result(rows, std::vector<int>(cols, 0));
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

bool salvarResultadoComTempo(const std::string &caminhoArquivo, const Matrix &matriz, long long tempoExecucaoMs) {
	std::ofstream novoArquivo(caminhoArquivo);
	if (!novoArquivo.is_open()) {
		return false;
	}

	int linhas = static_cast<int>(matriz.size());
	int colunas = linhas > 0 ? static_cast<int>(matriz[0].size()) : 0;
	novoArquivo << linhas << ' ' << colunas << '\n';

	for (int row = 0; row < linhas; ++row) {
		for (int col = 0; col < colunas; ++col) {
			novoArquivo << matriz[row][col];
			if (col + 1 < colunas) {
				novoArquivo << ' ';
			}
		}
		novoArquivo << '\n';
	}

	novoArquivo << "tempo_ms " << tempoExecucaoMs << '\n';
	return true;
}

int main(int argc, char *argv[]) {
	std::string caminhoMatriz1 = AppPaths::dirM1;
	std::string caminhoMatriz2 = AppPaths::dirM2;
	std::string caminhoResultado = AppPaths::dirResultadoSequencial;

	if (argc == 4) {
		caminhoMatriz1 = argv[1];
		caminhoMatriz2 = argv[2];
		caminhoResultado = argv[3];
	} else if (argc != 1) {
		std::cerr << "Uso: " << argv[0] << " [matriz_m1.txt matriz_m2.txt resultado.txt]\n";
		return 1;
	}

	Matrix m1;
	Matrix m2;
	if (!carregarArquivoMatriz(caminhoMatriz1, m1) || !carregarArquivoMatriz(caminhoMatriz2, m2)) {
		std::cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		std::cerr << "M1: " << caminhoMatriz1 << "\n";
		std::cerr << "M2: " << caminhoMatriz2 << "\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		std::cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	_mkdir(AppPaths::dirSaidaMatrizes);

	auto start = std::chrono::high_resolution_clock::now();
	Matrix result = multiplicarSequencialmente(m1, m2);
	auto end = std::chrono::high_resolution_clock::now();

	long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	if (!salvarResultadoComTempo(caminhoResultado, result, elapsedMs)) {
		std::cerr << "Erro: nao foi possivel salvar o resultado em arquivo.\n";
		return 1;
	}

	std::cout << "Resultado sequencial salvo em: " << caminhoResultado << "\n";
	std::cout << "Tempo de multiplicacao: " << elapsedMs << " ms\n";
	return 0;
}
