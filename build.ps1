$ErrorActionPreference = 'Stop'

$helperObject = Join-Path $env:TEMP 'locall_dr_sqlite_helper.o'
$output = Join-Path $PSScriptRoot 'LOCALLDR_SQLITE.exe'

gcc -c (Join-Path $PSScriptRoot 'sqlite_helper.c') -o $helperObject
if ($LASTEXITCODE -ne 0) {
    throw 'Falha ao compilar o helper SQLite.'
}

cobc -x -fstatic-call (Join-Path $PSScriptRoot 'LOCALLDR.cbl') $helperObject -o $output
if ($LASTEXITCODE -ne 0) {
    throw 'Falha ao compilar o programa COBOL.'
}

Write-Output "Executavel criado: $output"