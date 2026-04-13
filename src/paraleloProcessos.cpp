
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <direct.h>
#include <windows.h>

#include "constants.h"

using Matrix = std::vector<std::vector<int>>;

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

std::string caminhoArquivoParte(int indiceProcesso) {
	std::string base = AppPaths::dirResultadoParaleloProcessos;
	std::string sufixo = "_parte_" + std::to_string(indiceProcesso + 1) + ".txt";

	std::size_t pos = base.rfind(".txt");
	if (pos != std::string::npos) {
		return base.substr(0, pos) + sufixo;
	}

	return base + sufixo;
}

bool salvarResultadoParcial(const Matrix &resultadoParcial, long long tempoMs, const std::string &caminhoSaida) {
	std::ofstream arquivo(caminhoSaida);
	if (!arquivo.is_open()) {
		return false;
	}

	int linhas = static_cast<int>(resultadoParcial.size());
	int colunas = linhas > 0 ? static_cast<int>(resultadoParcial[0].size()) : 0;
	arquivo << linhas << ' ' << colunas << '\n';

	for (int row = 0; row < linhas; ++row) {
		for (int col = 0; col < colunas; ++col) {
			arquivo << resultadoParcial[row][col];
			if (col + 1 < colunas) {
				arquivo << ' ';
			}
		}
		arquivo << '\n';
	}

	arquivo << "tempo_ms " << tempoMs << '\n';
	return true;
}

bool lerTempoDoArquivo(const std::string &caminhoArquivo, long long &tempoMs) {
	std::ifstream arquivo(caminhoArquivo);
	if (!arquivo.is_open()) {
		return false;
	}

	std::string token;
	while (arquivo >> token) {
		if (token == "tempo_ms") {
			return static_cast<bool>(arquivo >> tempoMs);
		}
	}

	return false;
}

std::string quote(const std::string &value) {
	return "\"" + value + "\"";
}

int executarWorker(const std::string &caminhoM1, const std::string &caminhoM2, int inicio, int fim, const std::string &caminhoSaida) {
	Matrix m1;
	Matrix m2;
	if (!carregarArquivoMatriz(caminhoM1, m1) || !carregarArquivoMatriz(caminhoM2, m2)) {
		std::cerr << "Erro: worker nao conseguiu ler as matrizes.\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		std::cerr << "Erro: worker detectou dimensoes incompativeis.\n";
		return 1;
	}

	int colunas = static_cast<int>(m2[0].size());
	Matrix resultadoParcial(fim - inicio, std::vector<int>(colunas, 0));

	auto start = std::chrono::high_resolution_clock::now();
	for (int linha = inicio; linha < fim; ++linha) {
		for (int col = 0; col < colunas; ++col) {
			int sum = 0;
			for (int k = 0; k < static_cast<int>(m1[0].size()); ++k) {
				sum += m1[linha][k] * m2[k][col];
			}
			resultadoParcial[linha - inicio][col] = sum;
		}
	}
	auto end = std::chrono::high_resolution_clock::now();

	long long tempoMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	if (!salvarResultadoParcial(resultadoParcial, tempoMs, caminhoSaida)) {
		std::cerr << "Erro: worker nao conseguiu salvar o resultado parcial.\n";
		return 1;
	}

	return 0;
}

int executarPai(const std::string &caminhoM1, const std::string &caminhoM2, int totalProcessos) {
	Matrix m1;
	Matrix m2;

	if (!carregarArquivoMatriz(caminhoM1, m1) || !carregarArquivoMatriz(caminhoM2, m2)) {
		std::cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		std::cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	int n1 = static_cast<int>(m1.size());
	if (totalProcessos <= 0 || totalProcessos > n1) {
		std::cerr << "Erro: P deve estar no intervalo [1, N1].\n";
		return 1;
	}

	if (n1 % totalProcessos != 0) {
		std::cerr << "Erro: N1 deve ser divisivel por P para este modelo (N1/P linhas por processo).\n";
		return 1;
	}

	_mkdir(AppPaths::dirSaidaMatrizes);

	char exePath[MAX_PATH];
	if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) {
		std::cerr << "Erro: nao foi possivel obter o caminho do executavel.\n";
		return 1;
	}

	int bloco = n1 / totalProcessos;
	std::vector<HANDLE> handles;
	handles.reserve(totalProcessos);

	for (int i = 0; i < totalProcessos; ++i) {
		int inicio = i * bloco;
		int fim = inicio + bloco;
		std::string caminhoSaida = caminhoArquivoParte(i);

		std::ostringstream oss;
		oss << quote(exePath) << " --worker "
			<< quote(caminhoM1) << ' '
			<< quote(caminhoM2) << ' '
			<< inicio << ' '
			<< fim << ' '
			<< quote(caminhoSaida);

		std::string cmd = oss.str();
		std::vector<char> cmdBuffer(cmd.begin(), cmd.end());
		cmdBuffer.push_back('\0');

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		ZeroMemory(&pi, sizeof(pi));
		si.cb = sizeof(si);

		BOOL ok = CreateProcessA(
			nullptr,
			cmdBuffer.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			nullptr,
			&si,
			&pi
		);

		if (!ok) {
			std::cerr << "Erro: falha ao criar processo " << (i + 1) << ".\n";
			for (HANDLE h : handles) {
				WaitForSingleObject(h, INFINITE);
				CloseHandle(h);
			}
			return 1;
		}

		CloseHandle(pi.hThread);
		handles.push_back(pi.hProcess);
	}

	for (HANDLE h : handles) {
		WaitForSingleObject(h, INFINITE);
		CloseHandle(h);
	}

	long long tempoTotalMs = 0;
	for (int i = 0; i < totalProcessos; ++i) {
		long long tempoProcesso = 0;
		std::string caminhoSaida = caminhoArquivoParte(i);
		if (!lerTempoDoArquivo(caminhoSaida, tempoProcesso)) {
			std::cerr << "Erro: nao foi possivel ler o tempo do arquivo " << caminhoSaida << ".\n";
			return 1;
		}
		if (tempoProcesso > tempoTotalMs) {
			tempoTotalMs = tempoProcesso;
		}
	}

	std::cout << "Processamento paralelo com processos concluido.\n";
	std::cout << "Arquivos parciais gerados: " << totalProcessos << "\n";
	std::cout << "Tempo total (maior entre processos): " << tempoTotalMs << " ms\n";
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc >= 2 && std::string(argv[1]) == "--worker") {
		if (argc != 7) {
			std::cerr << "Erro: argumentos invalidos para modo worker.\n";
			return 1;
		}

		int inicio = 0;
		int fim = 0;
		if (!validarNumeroInteiro(argv[4], inicio) || !validarNumeroInteiro(argv[5], fim) || inicio < 0 || fim <= inicio) {
			std::cerr << "Erro: intervalo de linhas invalido no worker.\n";
			return 1;
		}

		return executarWorker(argv[2], argv[3], inicio, fim, argv[6]);
	}

	if (argc != 4) {
		std::cerr << "Uso: " << argv[0] << " matriz_m1.txt matriz_m2.txt P\n";
		return 1;
	}

	int totalProcessos = 0;
	if (!validarNumeroInteiro(argv[3], totalProcessos)) {
		std::cerr << "Erro: P deve ser um inteiro valido.\n";
		return 1;
	}

	return executarPai(argv[1], argv[2], totalProcessos);
}
