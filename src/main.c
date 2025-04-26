#include "../include/precompiler.h"

int main(int argc, char* argv[]) {
    // 1. Inizializza la struttura PreCompiler
    PreCompiler* compiler = init_precompiler();
    if (!compiler) {
        fprintf(stderr, "Errore: impossibile inizializzare il precompilatore\n");
        return 1;
    }
    
    // 2. Analizza gli argomenti della riga di comando
    if (parse_arguments(argc, argv, compiler) != 0) {
        free_precompiler(compiler);
        return 1;
    }
    
    // 3. Elabora il file
    if (!compiler || !compiler->input_filename) {
        fprintf(stderr, "Errore: parametri del compiler non validi\n");
        return 1;
    }
    
    // 4. Legge il contenuto del file di input
    int input_size, input_lines;
    char* content = read_file_content(compiler->input_filename, &input_size, &input_lines);
    if (!content) {
        return 1;
    }
    
    // 5. Imposta le statistiche del file di input
    compiler->stats.input_size = input_size;
    compiler->stats.input_lines = input_lines;
    
    // 6. Risolve le direttive #include
    char* content_with_includes = resolve_includes(content, compiler);
    free(content);
    if (!content_with_includes) {
        return 1;
    }
    
    // 7. Rimuove i commenti
    char* content_without_coments = remove_comments(content_with_includes, compiler);
    free(content_with_includes);
    if (!content_without_coments) {
        return 1;
    }

    // 8. Controlla i nomi delle variabili
    char* final_content = check_variables_name(content_without_coments, compiler);
    free(content_without_coments);
    if (!final_content) {
        return 1;
    }
    
    // 9. Calcola le statistiche finali
    compiler->stats.output_size = strlen(final_content);
    for (int i = 0; i < compiler->stats.output_size; i++) {
        if (final_content[i] == '\n') {
            compiler->stats.output_lines++;
        }
    }
    if (compiler->stats.output_size > 0 && final_content[compiler->stats.output_size - 1] != '\n') {
        compiler->stats.output_lines++;
    }
    
    // 10. Scrive l'output
    if (compiler->output_filename) {
        FILE* output_file = fopen(compiler->output_filename, "w");
        if (!output_file) {
            fprintf(stderr, "Errore: impossibile aprire il file di output %s\n", compiler->output_filename);
            free(final_content);
            return 1;
        }
        
        if (fputs(final_content, output_file) == EOF) {
            fprintf(stderr, "Errore: impossibile scrivere nel file di output %s\n", compiler->output_filename);
            fclose(output_file);
            free(final_content);
            return 1;
        }
        
        fclose(output_file);
    } else {
        // Scrive su stdout se non è specificato un file di output
        fputs(final_content, stdout);
    }
    
    // 11. Stampa le statistiche se richiesto
    if (compiler->verbose) {
        print_stats(compiler);
    }
    
    free(final_content);
    
    // 12. Libera la memoria
    free_precompiler(compiler);
    
    return 0;
} 