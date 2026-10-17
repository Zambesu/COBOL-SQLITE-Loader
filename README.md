# COBOL Local Loader

O programa importa um arquivo texto linha a linha para um banco SQLite. Cada linha vira um registro na tabela `records`, na coluna `content`. O banco e a tabela sao criados automaticamente; novas execucoes acrescentam registros. A importacao usa uma transacao e e desfeita se ocorrer um erro.

## Requisitos

- GnuCOBOL (`cobc`) e GCC no `PATH`.
- `winsqlite3.dll` disponivel na pasta de sistema do Windows.

## Compilar e executar

No PowerShell, na pasta do projeto:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
./LOCALLDR_SQLITE.exe
```

Informe primeiro o caminho do arquivo texto e depois o caminho do banco, por exemplo `dados.db`. O programa cria o banco se ele ainda nao existir.