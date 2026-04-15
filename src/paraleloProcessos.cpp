
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "constants.h"

using namespace std;

using Matrix = vector<vector<int>>;

struct resultadoProcesso {
	int workerIndex = 0;
	pid_t pid = -1;
	string outputPath;
};

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

string caminhoArquivoParte(int indiceProcesso) {
	string base = AppPaths::dirResultadoParaleloProcessos;
	string sufixo = "_parte_" + to_string(indiceProcesso + 1) + ".txt";

	size_t pos = base.rfind(".txt");
	if (pos != string::npos) {
		return base.substr(0, pos) + sufixo;
	}

	return base + sufixo;
}

bool salvarResultadoParcial(const Matrix &resultadoParcial, double tempoS, const string &caminhoSaida) {
	ofstream arquivo(caminhoSaida);
	if (!arquivo.is_open()) {
		return false;
	}

	int linhas = static_cast<int>(resultadoParcial.size());
	int colunas = linhas > 0 ? static_cast<int>(resultadoParcial[0].size()) : 0;
	arquivo << linhas << ' ' << colunas << '\n';

	for (int row = 0; row < linhas; ++row) {
		for (int col = 0; col < colunas; ++col) {
			arquivo << "c" << (row + 1) << (col + 1) << ' ' << resultadoParcial[row][col] << '\n';
		}
	}

	arquivo << tempoS << '\n';
	return true;
}

bool lerTempoResultadoParcial(const string &caminhoArquivo, double &tempoS) {
	ifstream arquivo(caminhoArquivo);
	if (!arquivo.is_open()) {
		return false;
	}

	int linhas = 0;
	int colunas = 0;
	if (!(arquivo >> linhas >> colunas) || linhas < 0 || colunas < 0) {
		return false;
	}

	string rotulo;
	int valor = 0;
	for (int i = 0; i < linhas * colunas; ++i) {
		if (!(arquivo >> rotulo >> valor)) {
			return false;
		}
	}

	if (!(arquivo >> tempoS)) {
		return false;
	}

	return true;
}

bool executarWorkerProcesso(const Matrix &m1, const Matrix &m2, int inicio, int fim, const string &caminhoSaida) {
	int colunas = static_cast<int>(m2[0].size());
	Matrix resultadoParcial(fim - inicio, vector<int>(colunas, 0));

	auto start = chrono::high_resolution_clock::now();
	for (int linha = inicio; linha < fim; ++linha) {
		for (int col = 0; col < colunas; ++col) {
			int sum = 0;
			for (int k = 0; k < static_cast<int>(m1[0].size()); ++k) {
				sum += m1[linha][k] * m2[k][col];
			}
			resultadoParcial[linha - inicio][col] = sum;
		}
	}
	auto end = chrono::high_resolution_clock::now();
	double tempoS = chrono::duration<double>(end - start).count();

	return salvarResultadoParcial(resultadoParcial, tempoS, caminhoSaida);
}

int executarComFork(const string &caminhoM1, const string &caminhoM2, int totalProcessos) {
	Matrix m1;
	Matrix m2;

	if (!carregarArquivoMatriz(caminhoM1, m1) || !carregarArquivoMatriz(caminhoM2, m2)) {
		cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	int n1 = static_cast<int>(m1.size());
	if (totalProcessos <= 0 || totalProcessos > n1) {
		cerr << "Erro: P deve estar no intervalo [1, N1].\n";
		return 1;
	}

	if (!criarDiretorioSaida(AppPaths::dirSaida)) {
		cerr << "Erro: nao foi possivel criar o diretorio de saida.\n";
		return 1;
	}

	if (!criarDiretorioSaida(AppPaths::dirSaidaMatrizes)) {
		cerr << "Erro: nao foi possivel criar o diretorio de saida.\n";
		return 1;
	}

	int blocoBase = n1 / totalProcessos;
	int resto = n1 % totalProcessos;
	vector<resultadoProcesso> resultados(totalProcessos);
	int inicioAtual = 0;

	for (int i = 0; i < totalProcessos; ++i) {
		int linhasWorker = blocoBase + (i >= totalProcessos - resto ? 1 : 0);
		int inicio = inicioAtual;
		int fim = inicio + linhasWorker;
		inicioAtual = fim;
		string caminhoSaida = caminhoArquivoParte(i);

		resultados[i].workerIndex = i;
		resultados[i].outputPath = caminhoSaida;

		pid_t pid = fork();
		if (pid < 0) {
			cerr << "Erro: falha ao criar processo " << (i + 1) << ".\n";
			for (int j = 0; j < i; ++j) {
				if (resultados[j].pid > 0) {
					wait(nullptr);
				}
			}
			return 1;
		}

		if (pid == 0) {
			bool sucesso = executarWorkerProcesso(m1, m2, inicio, fim, caminhoSaida);
			exit(sucesso ? 0 : 2);
		}

		resultados[i].pid = pid;
	}

	for (int i = 0; i < totalProcessos; ++i) {
		int status = 0;
		if (wait(&status) < 0) {
			cerr << "Erro: falha ao aguardar processo filho.\n";
			return 1;
		}

		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			cerr << "Erro: um processo filho finalizou com falha.\n";
			return 1;
		}
	}

	double tempoTotalS = 0.0;
	for (const resultadoProcesso &resultado : resultados) {
		double tempoWorkerS = 0.0;
		if (!lerTempoResultadoParcial(resultado.outputPath, tempoWorkerS)) {
			cerr << "Erro: nao foi possivel ler arquivo parcial de resultado.\n";
			return 1;
		}

		if (tempoWorkerS > tempoTotalS) {
			tempoTotalS = tempoWorkerS;
		}
	}

	cout << fixed << setprecision(6);
	cout << "Tempo total (maior entre workers): " << tempoTotalS << " s\n";
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		cerr << "Uso: " << argv[0] << " matriz_m1.txt matriz_m2.txt P\n";
		return 1;
	}

	int totalProcessos = 0;
	if (!validarNumeroInteiro(argv[3], totalProcessos)) {
		cerr << "Erro: P deve ser um inteiro valido.\n";
		return 1;
	}

	return executarComFork(argv[1], argv[2], totalProcessos);
}
