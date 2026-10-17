       IDENTIFICATION DIVISION.
       PROGRAM-ID. LOCALLDR.

       ENVIRONMENT DIVISION.
       INPUT-OUTPUT SECTION.
       FILE-CONTROL.
           SELECT INPUT-FILE ASSIGN TO INPUT-PATH
               ORGANIZATION IS LINE SEQUENTIAL
               FILE STATUS IS INPUT-STATUS.

       DATA DIVISION.
       FILE SECTION.
       FD INPUT-FILE RECORD IS VARYING IN SIZE FROM 0 TO 32767
           CHARACTERS DEPENDING ON RECORD-LENGTH.
       01 INPUT-RECORD PIC X(32767).

       WORKING-STORAGE SECTION.
       01 INPUT-PATH PIC X(260).
       01 DATABASE-PATH PIC X(260).
       01 INPUT-PATH-LENGTH PIC S9(9) COMP-5.
       01 DATABASE-PATH-LENGTH PIC S9(9) COMP-5.
       01 INPUT-STATUS PIC XX.
       01 SQLITE-STATUS PIC S9(9) COMP-5.
       01 RECORD-LENGTH PIC S9(9) COMP-5.
       01 RECORD-COUNT PIC 9(9) VALUE ZERO.

       PROCEDURE DIVISION.
       MAIN.
           DISPLAY 'Arquivo de entrada:'
           ACCEPT INPUT-PATH
           COMPUTE INPUT-PATH-LENGTH =
               FUNCTION LENGTH(FUNCTION TRIM(INPUT-PATH TRAILING))
           IF INPUT-PATH-LENGTH = ZERO
               DISPLAY 'Caminho de entrada vazio.'
               MOVE 1 TO RETURN-CODE
               STOP RUN
           END-IF

           DISPLAY 'Arquivo SQLite de destino:'
           ACCEPT DATABASE-PATH
           COMPUTE DATABASE-PATH-LENGTH =
               FUNCTION LENGTH(FUNCTION TRIM(DATABASE-PATH TRAILING))
           IF DATABASE-PATH-LENGTH = ZERO
               DISPLAY 'O caminho do banco de dados nao pode ser vazio.'
               MOVE 1 TO RETURN-CODE
               STOP RUN
           END-IF

           OPEN INPUT INPUT-FILE
           IF INPUT-STATUS NOT = '00'
               DISPLAY 'Nao foi possivel abrir o arquivo. Status: '
                   INPUT-STATUS
               MOVE 1 TO RETURN-CODE
               STOP RUN
           END-IF

           CALL 'LLDB_OPEN' USING
               BY REFERENCE DATABASE-PATH
               BY VALUE DATABASE-PATH-LENGTH
               RETURNING SQLITE-STATUS
           IF SQLITE-STATUS NOT = ZERO
               CALL 'LLDB_SHOW_ERROR'
               DISPLAY 'Falha ao iniciar o SQLite. Codigo: '
                   SQLITE-STATUS
               CALL 'LLDB_ABORT'
               CLOSE INPUT-FILE
               MOVE 1 TO RETURN-CODE
               STOP RUN
           END-IF

           PERFORM UNTIL INPUT-STATUS = '10'
               OR RETURN-CODE NOT = ZERO
               READ INPUT-FILE
               EVALUATE INPUT-STATUS
                   WHEN '00'
                       CALL 'LLDB_INSERT' USING
                           BY REFERENCE INPUT-RECORD
                           BY VALUE RECORD-LENGTH
                           RETURNING SQLITE-STATUS
                       IF SQLITE-STATUS NOT = ZERO
                           CALL 'LLDB_SHOW_ERROR'
                           DISPLAY 'Falha ao inserir registro. Codigo: '
                               SQLITE-STATUS
                           MOVE 1 TO RETURN-CODE
                       ELSE
                           ADD 1 TO RECORD-COUNT
                       END-IF
                   WHEN '10'
                       CONTINUE
                   WHEN OTHER
                       DISPLAY 'Erro ao ler o arquivo. Status: '
                           INPUT-STATUS
                       MOVE 1 TO RETURN-CODE
               END-EVALUATE
           END-PERFORM

           CLOSE INPUT-FILE
           IF RETURN-CODE = ZERO
               CALL 'LLDB_FINISH' RETURNING SQLITE-STATUS
               IF SQLITE-STATUS NOT = ZERO
                   CALL 'LLDB_SHOW_ERROR'
                   DISPLAY 'Falha ao concluir a transacao. Codigo: '
                       SQLITE-STATUS
                   MOVE 1 TO RETURN-CODE
               ELSE
                   DISPLAY 'Registros importados: ' RECORD-COUNT
               END-IF
           ELSE
               CALL 'LLDB_ABORT'
           END-IF
           STOP RUN.
