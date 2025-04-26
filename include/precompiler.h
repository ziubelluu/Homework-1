#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <getopt.h>

// Struttura per tenere traccia delle statistiche di elaborazione
typedef struct {
    int checked_vars;          // Numero di variabili controllate
    int errors_detected;       // Numero di errori rilevati
    int comment_lines_deleted; // Numero di righe di commento eliminate
    int files_included;        // Numero di file inclusi
    int input_lines;           // Numero di righe nel file di input
    int input_size;            // Dimensione in byte del file di input
    int output_lines;          // Numero di righe nel file di output
    int output_size;           // Dimensione in byte del file di output
} Stats;

// Struttura per tenere traccia degli errori di variabile
typedef struct {
    char* filename;            // Nome del file dove è stato rilevato l'errore
    int line_number;           // Numero di riga nel file
    char* var_name;            // Nome della variabile non valida
} InvalidVariable;

// Struttura per tenere traccia di un file incluso
typedef struct {
    char* filename;            // Nome del file incluso
    int size;                  // Dimensione in byte
    int lines;                 // Numero di righe
} IncludedFile;

// Struttura principale del programma
typedef struct {
    Stats stats;                          // Statistiche di elaborazione
    InvalidVariable** errors;             // Array di errori rilevati
    IncludedFile** included_files;        // Array di file inclusi
    char* input_filename;                 // Nome del file di input
    char* output_filename;                // Nome del file di output
    bool verbose;                         // Flag per l'output delle statistiche
} PreCompiler;

// Legge il contenuto di un file e restituisce una stringa
char *read_file_content(const char *filename, int *size, int *lines);

// Analizza gli argomenti della riga di comando
int parse_arguments(int argc, char* argv[], PreCompiler* compiler);

// Risolve le direttive #include
char* resolve_includes(const char* content, PreCompiler* compiler);

// Controlla la validità del nome variabili
char* check_variables_name(const char* content, PreCompiler* compiler);

// Rimuove tutti i commenti dal codice
char* remove_comments(const char* content, PreCompiler* compiler);

// Stampa le statistiche di elaborazione
void print_stats(const PreCompiler* compiler);

// Funzione di utilità per verificare se una variabile è valida
bool is_valid_name(const char* name);

// Inizializza la struttura PreCompiler
PreCompiler* init_precompiler(void);

// Libera la memoria allocata per la struttura PreCompiler
void free_precompiler(PreCompiler* compiler); 