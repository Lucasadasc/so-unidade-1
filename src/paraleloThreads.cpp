
#include <chrono>
#include <cerrno>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "constants.h"

using namespace std;

using Matrix = vector<vector<int>>;

struct resultadoThread {
	int threadId = 0;
	int linhaInicio = 0;
	int linhaFim = 0;
	double tempoS = 0.0;
};

struct tarefaThread {
	const Matrix *m1 = nullptr;
	const Matrix *m2 = nullptr;
	Matrix *resultado = nullptr;
	resultadoThread *threadInfo = nullptr;
	int linhaInicio = 0;
	int linhaFim = 0;
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

void executarBlocoLinhas(tarefaThread *task) {
	const Matrix &m1 = *(task->m1);
	const Matrix &m2 = *(task->m2);
	Matrix &resultado = *(task->resultado);
	resultadoThread &threadInfo = *(task->threadInfo);
	int linhaInicio = task->linhaInicio;
	int linhaFim = task->linhaFim;

	auto inicio = chrono::high_resolution_clock::now();

	int common = static_cast<int>(m1[0].size());
	int colunas = static_cast<int>(m2[0].size());

	for (int row = linhaInicio; row < linhaFim; ++row) {
		for (int col = 0; col < colunas; ++col) {
			int sum = 0;
			for (int k = 0; k < common; ++k) {
				sum += m1[row][k] * m2[k][col];
			}
			resultado[row][col] = sum;
		}
	}

	auto fim = chrono::high_resolution_clock::now();
	threadInfo.tempoS = chrono::duration<double>(fim - inicio).count();
}

void *multiplicarBlocoLinhas(void *param) {
	tarefaThread *task = static_cast<tarefaThread *>(param);
	executarBlocoLinhas(task);
	return nullptr;
}

string caminhoArquivoParte(int indiceThread) {
	string base = AppPaths::dirResultadoParaleloThreads;
	string sufixo = "_parte_" + to_string(indiceThread + 1) + ".txt";

	size_t pos = base.rfind(".txt");
	if (pos != string::npos) {
		return base.substr(0, pos) + sufixo;
	}

	return base + sufixo;
}

bool salvarResultadoParcial(const Matrix &resultado, const resultadoThread &info) {
	string caminho = caminhoArquivoParte(info.threadId);
	ofstream arquivo(caminho);
	if (!arquivo.is_open()) {
		return false;
	}

	int linhas = info.linhaFim - info.linhaInicio;
	int colunas = static_cast<int>(resultado[0].size());
	arquivo << linhas << ' ' << colunas << '\n';

	for (int row = info.linhaInicio; row < info.linhaFim; ++row) {
		for (int col = 0; col < colunas; ++col) {
			arquivo << "c" << (row + 1) << (col + 1) << ' ' << resultado[row][col] << '\n';
		}
	}

	arquivo << info.tempoS << '\n';
	return true;
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		cerr << "Passe os seguintes argumentos: matriz_m1.txt matriz_m2.txt T\n";
		return 1;
	}

	string caminhoMatriz1 = argv[1];
	string caminhoMatriz2 = argv[2];

	int totalThreads = 0;
	if (!validarNumeroInteiro(argv[3], totalThreads) || totalThreads <= 0) {
		cerr << "Erro: T deve ser um inteiro positivo.\n";
		return 1;
	}

	Matrix m1;
	Matrix m2;
	if (!carregarArquivoMatriz(caminhoMatriz1, m1) || !carregarArquivoMatriz(caminhoMatriz2, m2)) {
		cerr << "Erro: nao foi possivel ler os arquivos de matriz.\n";
		return 1;
	}

	if (m1.empty() || m2.empty() || m1[0].size() != m2.size()) {
		cerr << "Erro: dimensoes incompativeis para multiplicacao.\n";
		return 1;
	}

	int linhasResultado = static_cast<int>(m1.size());
	if (totalThreads > linhasResultado) {
		cerr << "Erro: T nao pode ser maior que o numero de linhas de M1 (N1).\n";
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

	int colunasResultado = static_cast<int>(m2[0].size());
	Matrix resultado(linhasResultado, vector<int>(colunasResultado, 0));

	int blocoBase = linhasResultado / totalThreads;
	int resto = linhasResultado % totalThreads;
	// criacao dos 'ids' das threads
	vector<pthread_t> threads(totalThreads);
	// vetor para controlar quais threads foram criadas com sucesso (para evitar join em threads não criadas)
	vector<bool> threadCriada(totalThreads, false);
	vector<tarefaThread> tasks(totalThreads);
	vector<resultadoThread> resultadosThreads(totalThreads);
	int inicioAtual = 0;

	for (int i = 0; i < totalThreads; ++i) {
		int linhasThread = blocoBase + (i >= totalThreads - resto ? 1 : 0);
		int inicio = inicioAtual;
		int fim = inicio + linhasThread;
		inicioAtual = fim;

		resultadosThreads[i].threadId = i;
		resultadosThreads[i].linhaInicio = inicio;
		resultadosThreads[i].linhaFim = fim;

		tasks[i].m1 = &m1;
		tasks[i].m2 = &m2;
		tasks[i].resultado = &resultado;
		tasks[i].threadInfo = &resultadosThreads[i];
		tasks[i].linhaInicio = inicio;
		tasks[i].linhaFim = fim;

		// endereço da thread / função de execução / parâmetro para a thread (indice do worker)
		int status = pthread_create(&threads[i], nullptr, multiplicarBlocoLinhas, &tasks[i]);
		if (status != 0) {
			cerr << "Erro: falha ao criar thread " << (i + 1) << ".\n";
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

	for (int i = 0; i < totalThreads; ++i) {
		if (threadCriada[i]) {
			pthread_join(threads[i], nullptr);
		}
	}

	double tempoTotalParaleloS = 0.0;
	for (const resultadoThread &info : resultadosThreads) {
		if (!salvarResultadoParcial(resultado, info)) {
			cerr << "Erro: nao foi possivel salvar um arquivo parcial de resultado.\n";
			return 1;
		}

		if (info.tempoS > tempoTotalParaleloS) {
			tempoTotalParaleloS = info.tempoS;
		}
	}

	cout << fixed << setprecision(6);
	cout << "Tempo total (maior entre threads): " << tempoTotalParaleloS << " s\n";

	return 0;
}

