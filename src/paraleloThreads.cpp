
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <direct.h>
#include <windows.h>

#include "constants.h"

using Matrix = std::vector<std::vector<int>>;

struct ThreadResult {
	int threadIndex = 0;
	int startRow = 0;
	int endRow = 0;
	long long elapsedMs = 0;
};

struct ThreadTask {
	const Matrix *m1 = nullptr;
	const Matrix *m2 = nullptr;
	Matrix *resultado = nullptr;
	ThreadResult *threadInfo = nullptr;
	int startRow = 0;
	int endRow = 0;
};

bool validarNumeroInteiro(const std::string &text, int &value) {
	try {
		std::size_t endPos = 0;
		int number = std::stoi(text, &endPos);
		if (endPos != text.size()) {
			return false;
		}
		value = number;
		return true;
	} catch (...) {
		return false;
	}
}

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

DWORD WINAPI multiplicarBlocoLinhas(LPVOID param) {
	ThreadTask *task = static_cast<ThreadTask *>(param);
	const Matrix &m1 = *(task->m1);
	const Matrix &m2 = *(task->m2);
	Matrix &resultado = *(task->resultado);
	ThreadResult &threadInfo = *(task->threadInfo);
	int startRow = task->startRow;
	int endRow = task->endRow;

	auto inicio = std::chrono::high_resolution_clock::now();

	int common = static_cast<int>(m1[0].size());
	int cols = static_cast<int>(m2[0].size());

	for (int row = startRow; row < endRow; ++row) {
		for (int col = 0; col < cols; ++col) {
			int sum = 0;
			for (int k = 0; k < common; ++k) {
				sum += m1[row][k] * m2[k][col];
			}
			resultado[row][col] = sum;
		}
	}

	auto fim = std::chrono::high_resolution_clock::now();
	threadInfo.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(fim - inicio).count();
	return 0;
}

std::string caminhoArquivoParte(int indiceThread) {
	std::string base = AppPaths::dirResultadoParaleloThreads;
	std::string sufixo = "_parte_" + std::to_string(indiceThread + 1) + ".txt";

	std::size_t pos = base.rfind(".txt");
	if (pos != std::string::npos) {
		return base.substr(0, pos) + sufixo;
	}

	return base + sufixo;
}

bool salvarResultadoParcial(const Matrix &resultado, const ThreadResult &info) {
	std::string caminho = caminhoArquivoParte(info.threadIndex);
	std::ofstream arquivo(caminho);
	if (!arquivo.is_open()) {
		return false;
	}

	int linhas = info.endRow - info.startRow;
	int colunas = static_cast<int>(resultado[0].size());
	arquivo << linhas << ' ' << colunas << '\n';

	for (int row = info.startRow; row < info.endRow; ++row) {
		for (int col = 0; col < colunas; ++col) {
			arquivo << resultado[row][col];
			if (col + 1 < colunas) {
				arquivo << ' ';
			}
		}
		arquivo << '\n';
	}

	arquivo << "tempo_ms " << info.elapsedMs << '\n';
	return true;
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		std::cerr << "Uso: " << argv[0] << " matriz_m1.txt matriz_m2.txt T\n";
		return 1;
	}

	std::string caminhoMatriz1 = argv[1];
	std::string caminhoMatriz2 = argv[2];

	int totalThreads = 0;
	if (!validarNumeroInteiro(argv[3], totalThreads) || totalThreads <= 0) {
		std::cerr << "Erro: T deve ser um inteiro positivo.\n";
		return 1;
	}

	Matrix m1;
	Matrix m2;
	if (!carregarArquivoMatriz(caminhoMatriz1, m1) || !carregarArquivoMatriz(caminhoMatriz2, m2)) {
		std::cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		std::cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	int linhasResultado = static_cast<int>(m1.size());
	if (totalThreads > linhasResultado) {
		std::cerr << "Erro: T nao pode ser maior que o numero de linhas de M1 (N1).\n";
		return 1;
	}

	if (linhasResultado % totalThreads != 0) {
		std::cerr << "Erro: N1 deve ser divisivel por T para este modelo (N1/T linhas por thread).\n";
		return 1;
	}

	_mkdir(AppPaths::dirSaidaMatrizes);

	int colunasResultado = static_cast<int>(m2[0].size());
	Matrix resultado(linhasResultado, std::vector<int>(colunasResultado, 0));

	int bloco = linhasResultado / totalThreads;
	std::vector<HANDLE> handles(totalThreads, nullptr);
	std::vector<ThreadTask> tasks(totalThreads);
	std::vector<ThreadResult> resultadosThreads(totalThreads);

	for (int i = 0; i < totalThreads; ++i) {
		int inicio = i * bloco;
		int fim = inicio + bloco;

		resultadosThreads[i].threadIndex = i;
		resultadosThreads[i].startRow = inicio;
		resultadosThreads[i].endRow = fim;

		tasks[i].m1 = &m1;
		tasks[i].m2 = &m2;
		tasks[i].resultado = &resultado;
		tasks[i].threadInfo = &resultadosThreads[i];
		tasks[i].startRow = inicio;
		tasks[i].endRow = fim;

		handles[i] = CreateThread(nullptr, 0, multiplicarBlocoLinhas, &tasks[i], 0, nullptr);
		if (handles[i] == nullptr) {
			std::cerr << "Erro: falha ao criar thread " << (i + 1) << ".\n";
			for (int j = 0; j < i; ++j) {
				if (handles[j] != nullptr) {
					WaitForSingleObject(handles[j], INFINITE);
					CloseHandle(handles[j]);
				}
			}
			return 1;
		}
	}

	for (HANDLE h : handles) {
		WaitForSingleObject(h, INFINITE);
		CloseHandle(h);
	}

	long long tempoTotalParaleloMs = 0;
	for (const ThreadResult &info : resultadosThreads) {
		if (!salvarResultadoParcial(resultado, info)) {
			std::cerr << "Erro: nao foi possivel salvar um arquivo parcial de resultado.\n";
			return 1;
		}

		if (info.elapsedMs > tempoTotalParaleloMs) {
			tempoTotalParaleloMs = info.elapsedMs;
		}
	}

	std::cout << "Processamento paralelo com threads concluido.\n";
	std::cout << "Arquivos parciais gerados: " << totalThreads << "\n";
	std::cout << "Tempo total (maior entre threads): " << tempoTotalParaleloMs << " ms\n";

	return 0;
}

