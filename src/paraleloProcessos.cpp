
#include <chrono>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>
#include <direct.h>

#include "constants.h"

using namespace std;

using Matrix = vector<vector<int>>;

struct WorkerResult {
	int workerIndex = 0;
	double elapsedS = 0.0;
	bool success = true;
	string outputPath;
};

struct WorkerTask {
	const Matrix *m1 = nullptr;
	const Matrix *m2 = nullptr;
	int inicio = 0;
	int fim = 0;
	string caminhoSaida;
	WorkerResult *result = nullptr;
};

bool criarDiretorioSaida(const char *dir) {
	int status = _mkdir(dir);
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

void *executarWorker(void *param) {
	WorkerTask *task = static_cast<WorkerTask *>(param);
	const Matrix &m1 = *(task->m1);
	const Matrix &m2 = *(task->m2);
	WorkerResult &result = *(task->result);

	int colunas = static_cast<int>(m2[0].size());
	Matrix resultadoParcial(task->fim - task->inicio, vector<int>(colunas, 0));

	auto start = chrono::high_resolution_clock::now();
	for (int linha = task->inicio; linha < task->fim; ++linha) {
		for (int col = 0; col < colunas; ++col) {
			int sum = 0;
			for (int k = 0; k < static_cast<int>(m1[0].size()); ++k) {
				sum += m1[linha][k] * m2[k][col];
			}
			resultadoParcial[linha - task->inicio][col] = sum;
		}
	}
	auto end = chrono::high_resolution_clock::now();

	result.elapsedS = chrono::duration<double>(end - start).count();
	if (!salvarResultadoParcial(resultadoParcial, result.elapsedS, task->caminhoSaida)) {
		result.success = false;
	}

	return nullptr;
}

int executarComPthreads(const string &caminhoM1, const string &caminhoM2, int totalWorkers) {
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
	if (totalWorkers <= 0 || totalWorkers > n1) {
		cerr << "Erro: P deve estar no intervalo [1, N1].\n";
		return 1;
	}

	if (!criarDiretorioSaida(AppPaths::dirSaidaMatrizes)) {
		cerr << "Erro: nao foi possivel criar o diretorio de saida.\n";
		return 1;
	}

	int blocoBase = n1 / totalWorkers;
	int resto = n1 % totalWorkers;
	vector<pthread_t> threads(totalWorkers);
	vector<bool> threadCriada(totalWorkers, false);
	vector<WorkerTask> tasks(totalWorkers);
	vector<WorkerResult> resultados(totalWorkers);
	int inicioAtual = 0;

	for (int i = 0; i < totalWorkers; ++i) {
		int linhasWorker = blocoBase + (i >= totalWorkers - resto ? 1 : 0);
		int inicio = inicioAtual;
		int fim = inicio + linhasWorker;
		inicioAtual = fim;
		string caminhoSaida = caminhoArquivoParte(i);

		resultados[i].workerIndex = i;
		resultados[i].outputPath = caminhoSaida;

		tasks[i].m1 = &m1;
		tasks[i].m2 = &m2;
		tasks[i].inicio = inicio;
		tasks[i].fim = fim;
		tasks[i].caminhoSaida = caminhoSaida;
		tasks[i].result = &resultados[i];

		// endereço da thread / função de execução / parâmetro para a thread (indice do worker)
		int status = pthread_create(&threads[i], nullptr, executarWorker, &tasks[i]);
		if (status != 0) {
			cerr << "Erro: falha ao criar worker " << (i + 1) << ".\n";
			for (int j = 0; j < i; ++j) {
				if (threadCriada[j]) {
					// especie de await - dizemos qual thread queremos esperar e o que fazer quando ela terminar (nullptr = não precisamos de retorno)
					pthread_join(threads[j], nullptr);
				}
			}
			return 1;
		}

		threadCriada[i] = true;
	}

	for (int i = 0; i < totalWorkers; ++i) {
		if (threadCriada[i]) {
			pthread_join(threads[i], nullptr);
		}
	}

	double tempoTotalS = 0.0;
	for (const WorkerResult &result : resultados) {
		if (!result.success) {
			cerr << "Erro: nao foi possivel salvar um arquivo parcial de resultado.\n";
			return 1;
		}

		if (result.elapsedS > tempoTotalS) {
			tempoTotalS = result.elapsedS;
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

	return executarComPthreads(argv[1], argv[2], totalProcessos);
}
