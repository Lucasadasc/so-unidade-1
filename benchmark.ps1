$valores = @(2, 4, 8, 16)
$resultadosFile = "resultados_benchmark.txt"

# Limpar arquivo anterior
if (Test-Path $resultadosFile) {
    Remove-Item $resultadosFile
}

# Sequencial - 10 vezes
Write-Host "=== SEQUENCIAL ===" -ForegroundColor Green | Tee-Object -FilePath $resultadosFile -Append
for ($i = 1; $i -le 10; $i++) {
    Write-Host "Iteração $i" -ForegroundColor Yellow | Tee-Object -FilePath $resultadosFile -Append
    & .\src\output\sequencial.exe src/output/matrizes/matriz_m1.txt src/output/matrizes/matriz_m2.txt | Tee-Object -FilePath $resultadosFile -Append
}

# Threads - 10 vezes para cada valor
foreach ($valor in $valores) {
    Write-Host "`n=== THREADS COM VALOR: $valor ===" -ForegroundColor Cyan | Tee-Object -FilePath $resultadosFile -Append
    for ($i = 1; $i -le 10; $i++) {
        Write-Host "  Valor=$valor | Iteração=$i" -ForegroundColor Yellow | Tee-Object -FilePath $resultadosFile -Append
        & .\src\output\paraleloThreads.exe src/output/matrizes/matriz_m1.txt src/output/matrizes/matriz_m2.txt $valor | Tee-Object -FilePath $resultadosFile -Append
    }
}

# Processos - 10 vezes para cada valor
foreach ($valor in $valores) {
    Write-Host "`n=== PROCESSOS COM VALOR: $valor ===" -ForegroundColor Magenta | Tee-Object -FilePath $resultadosFile -Append
    for ($i = 1; $i -le 10; $i++) {
        Write-Host "  Valor=$valor | Iteração=$i" -ForegroundColor Yellow | Tee-Object -FilePath $resultadosFile -Append
        & .\src\output\paraleloProcessos.exe src/output/matrizes/matriz_m1.txt src/output/matrizes/matriz_m2.txt $valor | Tee-Object -FilePath $resultadosFile -Append
    }
}

Write-Host "`n[OK] Benchmark concluido! Resultados salvos em $resultadosFile" -ForegroundColor Green
