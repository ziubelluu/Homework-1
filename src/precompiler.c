#include "../include/precompiler.h"

// Inizializza la struttura PreCompiler
PreCompiler *init_precompiler()
{
    PreCompiler *compiler = (PreCompiler *)malloc(sizeof(PreCompiler));
    if (!compiler)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per PreCompiler\n");
        return NULL;
    }

    /// Inizializza i valori
    memset(&compiler->stats, 0, sizeof(Stats));
    compiler->errors = NULL;
    compiler->included_files = NULL;
    compiler->input_filename = NULL;
    compiler->output_filename = NULL;
    compiler->verbose = false;

    return compiler;
}

// Libera la memoria allocata per la struttura PreCompiler
void free_precompiler(PreCompiler *compiler)
{
    if (!compiler)
        return;

    // Libera la memoria per gli errori
    if (compiler->errors)
    {
        for (int i = 0; i < compiler->stats.errors_detected; i++)
        {
            if (compiler->errors[i])
            {
                free(compiler->errors[i]->filename);
                free(compiler->errors[i]->var_name);
                free(compiler->errors[i]);
            }
        }
        free(compiler->errors);
    }

    // Libera la memoria per i file inclusi
    if (compiler->included_files)
    {
        for (int i = 0; i < compiler->stats.files_included; i++)
        {
            if (compiler->included_files[i])
            {
                free(compiler->included_files[i]->filename);
                free(compiler->included_files[i]);
            }
        }
        free(compiler->included_files);
    }

    // Libera i nomi dei file se allocati
    if (compiler->input_filename)
        free(compiler->input_filename);
    if (compiler->output_filename)
        free(compiler->output_filename);

    // Libera la struttura principale
    free(compiler);
}

// Funzione per verificare se il nome di una variabile è valido
bool is_valid_name(const char *name)
{
    if (!name || !*name)
        return false;
    // Il primo carattere deve essere una lettera o underscore
    if (!isalpha((unsigned char)name[0]) && name[0] != '_')
    {
        return false;
    }

    // I caratteri successivi devono essere lettere, numeri o underscore
    for (int i = 1; name[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_')
        {
            return false;
        }
    }

    return true;
}

// Analizza gli argomenti della riga di comando
int parse_arguments(int argc, char *argv[], PreCompiler *compiler)
{
    int option;
    int option_index = 0;

    static struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"out", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {0, 0, 0, 0}};

    // Elabora le opzioni della riga di comando
    while ((option = getopt_long(argc, argv, "i:o:v", long_options, &option_index)) != -1)
    {
        switch (option)
        {
        case 'i':
            compiler->input_filename = strdup(optarg);
            break;
        case 'o':
            compiler->output_filename = strdup(optarg);
            break;
        case 'v':
            compiler->verbose = true;
            break;
        default:
            fprintf(stderr, "Opzione sconosciuta: %c\n", option);
            return 1;
        }
    }

    // Verifica che sia stato specificato un file di input
    if (!compiler->input_filename)
    {
        fprintf(stderr, "Errore: il file di input è obbligatorio\n");
        fprintf(stderr, "Uso: %s [-i|--in file_input] [-o|--out file_output] [-v|--verbose]\n", argv[0]);
        return 1;
    }

    return 0;
}

// Funzione per leggere il contenuto di un file
char *read_file_content(const char *filename, int *size, int *lines)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        fprintf(stderr, "Errore: impossibile aprire il file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = (char *)malloc(file_size + 1);
    if (!content)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per il contenuto del file\n");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, file);
    content[read_size] = '\0';

    int line_count = 0;
    for (size_t i = 0; i < read_size; i++)
    {
        if (content[i] == '\n')
        {
            line_count++;
        }
    }

    if (read_size > 0 && content[read_size - 1] != '\n')
    {
        line_count++;
    }

    if (size)
        *size = (int)read_size;
    if (lines)
        *lines = line_count;

    fclose(file);
    return content;
}

// Risolve gli #include
char *resolve_includes(const char *content, PreCompiler *compiler)
{
    if (!content)
        return NULL;

    size_t result_capacity = strlen(content);
    char *result = (char *)malloc(result_capacity + 1);
    if (!result)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per il risultato\n");
        return NULL;
    }
    result[0] = '\0';

    // Preparazione per l'array di file inclusi
    int included_files_capacity = 1;
    if (compiler->included_files == NULL)
    {
        compiler->included_files = (IncludedFile **)malloc(included_files_capacity * sizeof(IncludedFile *));
        if (!compiler->included_files)
        {
            fprintf(stderr, "Errore: impossibile allocare memoria per i file inclusi\n");
            free(result);
            return NULL;
        }
    }

    const char *ptr = content;
    size_t result_len = 0;

    // Elabora il contenuto linea per linea
    while (*ptr)
    {
        // Cerca l'inizio della prossima linea
        const char *line_start = ptr;
        const char *line_end = strchr(ptr, '\n');
        if (!line_end)
        {
            // Ultima linea senza newline
            line_end = ptr + strlen(ptr);
        }
        else
        {
            line_end++; // Include il newline
        }

        // Verifica se la linea contiene una direttiva #include
        if (strncmp(line_start, "#include", 8) == 0 && isspace((unsigned char)line_start[8]))
        {
            // Trova il nome del file tra virgolette o parentesi angolari
            const char *include_start = strpbrk(line_start, "\"<");
            const char *include_end = NULL;

            if (include_start)
            {
                include_start++; // Salta la virgoletta o parentesi angolare iniziale
                include_end = strpbrk(include_start, "\">");
            }

            if (include_start && include_end)
            {
                // Estrai il nome del file
                size_t filename_len = include_end - include_start;
                char *include_filename = (char *)malloc(filename_len + 1);
                if (!include_filename)
                {
                    fprintf(stderr, "Errore: impossibile allocare memoria per il nome del file incluso\n");
                    free(result);
                    return NULL;
                }

                strncpy(include_filename, include_start, filename_len);
                include_filename[filename_len] = '\0';

                // Controlla se il file è già stato incluso per evitare inclusioni cicliche
                bool already_included = false;
                for (int i = 0; i < compiler->stats.files_included; i++)
                {
                    if (strcmp(compiler->included_files[i]->filename, include_filename) == 0)
                    {
                        already_included = true;
                        break;
                    }
                }

                if (!already_included)
                {
                    // Legge il contenuto del file incluso
                    int file_size, file_lines;
                    char *include_content = read_file_content(include_filename, &file_size, &file_lines);
                    if (include_content)
                    {
                        // Aggiunge una nuova IncludedFile alla struttura
                        if (compiler->stats.files_included >= included_files_capacity)
                        {
                            included_files_capacity *= 2;
                            IncludedFile **new_included_files = (IncludedFile **)realloc(compiler->included_files, included_files_capacity * sizeof(IncludedFile *));
                            if (!new_included_files)
                            {
                                fprintf(stderr, "Errore: impossibile riallocare memoria per i file inclusi\n");
                                free(include_filename);
                                free(include_content);
                                free(result);
                                return NULL;
                            }
                            compiler->included_files = new_included_files;
                        }

                        IncludedFile *included_file = (IncludedFile *)malloc(sizeof(IncludedFile));
                        if (!included_file)
                        {
                            fprintf(stderr, "Errore: impossibile allocare memoria per il file incluso\n");
                            free(include_filename);
                            free(include_content);
                            free(result);
                            return NULL;
                        }

                        included_file->filename = include_filename;
                        included_file->size = file_size;
                        included_file->lines = file_lines;
                        compiler->included_files[compiler->stats.files_included++] = included_file;

                        // Elabora ricorsivamente il contenuto del file incluso per gestire gli include nidificati
                        char *processed_include_content = resolve_includes(include_content, compiler);
                        if (processed_include_content)
                        {
                            // Aggiunge il contenuto processato al risultato
                            size_t content_len = strlen(processed_include_content);
                            if (result_len + content_len + 1 > result_capacity)
                            {
                                result_capacity = (result_len + content_len) * 2;
                                char *new_result = (char *)realloc(result, result_capacity);
                                if (!new_result)
                                {
                                    fprintf(stderr, "Errore: impossibile riallocare memoria per il risultato\n");
                                    free(processed_include_content);
                                    free(include_content);
                                    free(result);
                                    return NULL;
                                }
                                result = new_result;
                            }

                            strcpy(result + result_len, processed_include_content);
                            result_len += content_len;

                            free(processed_include_content);
                        }
                        free(include_content);
                    }
                    else
                    {
                        fprintf(stderr, "Avviso: impossibile includere il file %s\n", include_filename);
                        free(include_filename);
                    }
                }
                else
                {
                    // Il file è già stato incluso, ignoriamo per evitare un loop di inclusioni
                    free(include_filename);
                }
            }
            else
            {
                // Copia la linea originale se il formato è errato
                size_t line_len = line_end - line_start;
                if (result_len + line_len + 1 > result_capacity)
                {
                    result_capacity = (result_len + line_len) * 2;
                    char *new_result = (char *)realloc(result, result_capacity);
                    if (!new_result)
                    {
                        fprintf(stderr, "Errore: impossibile riallocare memoria per il risultato\n");
                        free(result);
                        return NULL;
                    }
                    result = new_result;
                }

                strncpy(result + result_len, line_start, line_len);
                result_len += line_len;
                result[result_len] = '\0';
            }
        }
        else
        {
            // Copia la linea originale
            size_t line_len = line_end - line_start;
            if (result_len + line_len + 1 > result_capacity)
            {
                result_capacity = (result_len + line_len) * 2;
                char *new_result = (char *)realloc(result, result_capacity);
                if (!new_result)
                {
                    fprintf(stderr, "Errore: impossibile riallocare memoria per il risultato\n");
                    free(result);
                    return NULL;
                }
                result = new_result;
            }

            strncpy(result + result_len, line_start, line_len);
            result_len += line_len;
            result[result_len] = '\0';
        }

        ptr = line_end;
    }

    return result;
}

// Controlla la validità degli identificatori di variabili
char *check_variables_name(const char *content, PreCompiler *compiler)
{
    if (!content)
        return NULL;

    // Alloca memoria per il risultato e copia il contenuto
    char *result = strdup(content);
    if (!result)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per il risultato\n");
        return NULL;
    }

    // Preparazione per l'array di errori
    compiler->errors = (InvalidVariable **)malloc(sizeof(InvalidVariable *));
    if (!compiler->errors)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per gli errori\n");
        free(result);
        return NULL;
    }

    // Analizza il codice riga per riga per trovare dichiarazioni di variabili
    int line_number = 1;
    const char *ptr = content;  
    bool has_previous_line_ended = true;
    bool is_in_braces = false;
    
    while (*ptr)
    {
        
        // Se troviamo un newline, incrementiamo il contatore di righe                        
        if (*ptr == '\n') {
            line_number++;
        } else if (*ptr == ';' || *ptr == '{' || *ptr == '}') {
            // Gestione di fine riga con punto e virgola e parentesi graffa
            has_previous_line_ended = true;
        } else if (*ptr == '(') {
            // Se troviamo una parentesi aperta, impostiamo il flag per indicare che siamo all'interno di una parentesi
            is_in_braces = true;
        }

        // Cerca dichiarazioni di variabili se la riga precedente è terminata        
        if ((isalpha((unsigned char)*ptr)) && has_previous_line_ended)
        {
            // Cerca il tipo (int, char, float, ecc.)
            const char *type_start = ptr;
            while (isalnum((unsigned char)*ptr)) {
                ptr++;    
                if (*ptr == '\n') {
                    line_number++;
                }
            }

            // Estrae il tipo
            size_t type_len = ptr - type_start;
            char type_name[64] = {0};
            if (type_len < sizeof(type_name))
            {
                strncpy(type_name, type_start, type_len);
                type_name[type_len] = '\0';
                
                // Verifica se è un tipo C comune
                if (strcmp(type_name, "int") == 0 ||
                    strcmp(type_name, "char") == 0 ||
                    strcmp(type_name, "float") == 0 ||
                    strcmp(type_name, "double") == 0 ||
                    strcmp(type_name, "void") == 0 ||
                    strcmp(type_name, "long") == 0 ||
                    strcmp(type_name, "short") == 0 ||
                    strcmp(type_name, "unsigned") == 0 ||
                    strcmp(type_name, "signed") == 0 ||
                    strcmp(type_name, "struct") == 0 ||
                    strcmp(type_name, "enum") == 0 ||
                    strcmp(type_name, "union") == 0)
                {
                    // Cerca le variabili
                    bool multiple_variables = false;
                    do {
                        multiple_variables = false;
                        // Salta spazi bianchi dopo il tipo
                        while (isspace((unsigned char)*ptr) || *ptr == '*') {
                            ptr++;                            
                            if (*ptr == '\n') {
                                line_number++;
                            } 
                        }
                        // Inizio del nome della variabile
                        const char *var_start = ptr;

                        // Raccogli tutti i caratteri che potrebbero far parte del nome
                        // Include caratteri non validi per evidenziare gli errori
                        while (*ptr && *ptr != ',' && *ptr != ';' && *ptr != '=' && *ptr != '(' && *ptr != '[' && *ptr != ']'){ 
                            if (*(ptr + 1) == ',') {
                                multiple_variables = true;
                            }
                            if (*ptr == ')' && is_in_braces) {
                                ptr--;
                                is_in_braces = false;
                                break;
                            } else {
                                ptr++;
                            }
                            if (*ptr == '\n') {
                                line_number++;
                            } 
                            
                        }

                        // Estrae il nome della variabile
                        size_t var_len = ptr - var_start;
                        char var_name[256] = {0};
                        
                        if (var_len > 0 && var_len < sizeof(var_name))
                        {
                            strncpy(var_name, var_start, var_len);
                            
                            var_name[var_len] = '\0';

                            compiler->stats.checked_vars++;
                            
                            if (!is_valid_name(var_name))
                            {
                                // Aggiunge un errore
                                InvalidVariable **new_errors = (InvalidVariable **)realloc(compiler->errors, (compiler->stats.errors_detected + 1) * sizeof(InvalidVariable *));
                                if (!new_errors)
                                {
                                    fprintf(stderr, "Errore: impossibile riallocare memoria per gli errori\n");
                                    free(result);
                                    return NULL;
                                }
                                compiler->errors = new_errors;
                                

                                InvalidVariable *error = (InvalidVariable *)malloc(sizeof(InvalidVariable));
                                if (!error)
                                {
                                    fprintf(stderr, "Errore: impossibile allocare memoria per l'errore\n");
                                    free(result);
                                    return NULL;
                                }
                                error->filename = strdup(compiler->input_filename);
                                error->line_number = line_number;
                                error->var_name = strdup(var_name);
                                compiler->errors[compiler->stats.errors_detected++] = error;
                            }
                            has_previous_line_ended = !multiple_variables;
                        }
                        if (*ptr && multiple_variables)
                            ptr++;
                    } while (multiple_variables);
                }   
            }
        }
        // Passa al carattere successivo
        if (*ptr)
            ptr++;
    }

    return result;
}

// Rimuove tutti i commenti dal codice
char *remove_comments(const char *content, PreCompiler *compiler)
{
    if (!content)
        return NULL;

    // Alloca memoria per il risultato
    size_t content_len = strlen(content);
    char *result = (char *)malloc(content_len + 1);
    if (!result)
    {
        fprintf(stderr, "Errore: impossibile allocare memoria per il risultato\n");
        return NULL;
    }

    // Copia il contenuto rimuovendo i commenti
    size_t result_len = 0;
    bool in_line_comment = false;
    bool in_block_comment = false;

    for (size_t i = 0; i < content_len; i++)
    {
        if (in_line_comment)
        {
            // In un commento di linea, continua fino a un newline
            if (content[i] == '\n')
            {
                in_line_comment = false;
                result[result_len++] = content[i];
            }
        }
        else if (in_block_comment)
        {
            // In un commento di blocco, controlla la fine del commento
            if (content[i] == '*' && i + 1 < content_len && content[i + 1] == '/')
            {
                in_block_comment = false;
                i++; // Salta il carattere '/'
                compiler->stats.comment_lines_deleted++;
            }
            else if (content[i] == '\n')
            {
                result[result_len++] = content[i];
                compiler->stats.comment_lines_deleted++;
            }
        }
        else
        {
            // Non in un commento, verifica l'inizio di un commento
            if (content[i] == '/' && i + 1 < content_len)
            {
                if (content[i + 1] == '/')
                {
                    // Commento di linea
                    in_line_comment = true;
                    compiler->stats.comment_lines_deleted++;
                    i++; // Salta il secondo carattere '/'
                }
                else if (content[i + 1] == '*')
                {
                    // Commento di blocco
                    in_block_comment = true;
                    i++; // Salta il carattere '*'
                }
                else
                {
                    // Non è un commento, copia normalmente
                    result[result_len++] = content[i];
                }
            }
            else
            {
                // Copia normalmente
                result[result_len++] = content[i];
            }
        }
    }

    result[result_len] = '\0';
    return result;
}

// Funzione per calcolare la larghezza massima per una colonna
size_t get_max_width(const char *header, const char **values, int count) {
    size_t max_width = strlen(header);
    
    for (int i = 0; i < count; i++) {
        size_t value_length = values[i] ? strlen(values[i]) : 0;
        if (value_length > max_width) {
            max_width = value_length;
        }
    }
    
    return max_width + 2; // Aggiungi spaziatura
}

// Stampa una linea di separazione per la tabella
void print_table_separator(size_t *widths, int num_columns) {
    for (int i = 0; i < num_columns; i++) {
        char s[widths[i]];
        memset(s, '-', widths[i]);
        fprintf(stdout, "+%.*s", (int)widths[i], s);
    }
    fprintf(stdout, "+\n");
}

// Stampa l'intestazione della tabella
void print_table_header(const char **headers, size_t *widths, int num_columns) {
    for (int i = 0; i < num_columns; i++) {
        fprintf(stdout, "|%-*s", (int)widths[i], headers[i]);
    }
    fprintf(stdout, "|\n");
}

// Stampa una riga della tabella
void print_table_row(const char **values, size_t *widths, int num_columns) {
    for (int i = 0; i < num_columns; i++) {
        fprintf(stdout, "|%-*s", (int)widths[i], values[i]);
    }
    fprintf(stdout, "|\n");
}

// Stampa le statistiche di elaborazione
void print_stats(const PreCompiler *compiler)
{
    if (!compiler)
        return;

    fprintf(stdout, "\n==== Statistiche di elaborazione ====\n");
    fprintf(stdout, "Numero di variabili controllate: %d\n", compiler->stats.checked_vars);
    fprintf(stdout, "Numero di errori rilevati: %d\n", compiler->stats.errors_detected);

    // Stampa i dettagli degli errori
    if (compiler->stats.errors_detected > 0)
    {
        fprintf(stdout, "\nDettaglio degli errori rilevati:\n");
        
        // Prepara le intestazioni
        const char *headers[] = {"File", "Linea", "Variabile"};
        int num_columns = 3;
        
        // Prepara i dati per il calcolo delle larghezze
        char **filenames = malloc(compiler->stats.errors_detected * sizeof(char*));
        char **line_numbers = malloc(compiler->stats.errors_detected * sizeof(char*));
        char **var_names = malloc(compiler->stats.errors_detected * sizeof(char*));
        
        for (int i = 0; i < compiler->stats.errors_detected; i++) {
            filenames[i] = compiler->errors[i]->filename;
            
            // Converti i numeri di linea in stringhe
            line_numbers[i] = malloc(20);
            sprintf(line_numbers[i], "%d", compiler->errors[i]->line_number);
            
            var_names[i] = compiler->errors[i]->var_name;
        }
        
        // Calcola le larghezze delle colonne
        size_t widths[3];
        widths[0] = get_max_width(headers[0], (const char**)filenames, compiler->stats.errors_detected);
        widths[1] = get_max_width(headers[1], (const char**)line_numbers, compiler->stats.errors_detected);
        widths[2] = get_max_width(headers[2], (const char**)var_names, compiler->stats.errors_detected);
        
        // Stampa la tabella
        print_table_separator(widths, num_columns);
        print_table_header(headers, widths, num_columns);
        print_table_separator(widths, num_columns);
        
        for (int i = 0; i < compiler->stats.errors_detected; i++) {
            const char *values[] = {
                compiler->errors[i]->filename,
                line_numbers[i],
                compiler->errors[i]->var_name
            };
            print_table_row(values, widths, num_columns);
            print_table_separator(widths, num_columns);
        }
        
        // Libera la memoria allocata
        for (int i = 0; i < compiler->stats.errors_detected; i++) {
            free(line_numbers[i]);
        }
        free(filenames);
        free(line_numbers);
        free(var_names);
    }

    fprintf(stdout, "\nNumero di righe di commento eliminate: %d\n", compiler->stats.comment_lines_deleted);
    fprintf(stdout, "Numero di file inclusi: %d\n", compiler->stats.files_included);

    // Stampa i dettagli dei file inclusi
    if (compiler->stats.files_included > 0)
    {
        fprintf(stdout, "\nDettaglio dei file inclusi:\n");
        
        // Prepara le intestazioni
        const char *headers[] = {"File", "Dimensione", "Righe"};
        int num_columns = 3;
        
        // Prepara i dati per il calcolo delle larghezze
        char **filenames = malloc(compiler->stats.files_included * sizeof(char*));
        char **sizes = malloc(compiler->stats.files_included * sizeof(char*));
        char **lines = malloc(compiler->stats.files_included * sizeof(char*));
        
        for (int i = 0; i < compiler->stats.files_included; i++) {
            filenames[i] = compiler->included_files[i]->filename;
            
            // Converte dimensioni e righe in stringhe
            sizes[i] = malloc(20);
            sprintf(sizes[i], "%d", compiler->included_files[i]->size);
            
            lines[i] = malloc(20);
            sprintf(lines[i], "%d", compiler->included_files[i]->lines);
        }
        
        // Calcola le larghezze delle colonne
        size_t widths[3];
        widths[0] = get_max_width(headers[0], (const char**)filenames, compiler->stats.files_included);
        widths[1] = get_max_width(headers[1], (const char**)sizes, compiler->stats.files_included);
        widths[2] = get_max_width(headers[2], (const char**)lines, compiler->stats.files_included);
        
        // Stampa la tabella
        print_table_separator(widths, num_columns);
        print_table_header(headers, widths, num_columns);
        print_table_separator(widths, num_columns);
        
        for (int i = 0; i < compiler->stats.files_included; i++) {
            const char *values[] = {
                compiler->included_files[i]->filename,
                sizes[i],
                lines[i]
            };
            print_table_row(values, widths, num_columns);
            print_table_separator(widths, num_columns);
        }
        
        // Libera la memoria allocata
        for (int i = 0; i < compiler->stats.files_included; i++) {
            free(sizes[i]);
            free(lines[i]);
        }
        free(filenames);
        free(sizes);
        free(lines);
    }

    fprintf(stdout, "\nFile di input (%s):\n", compiler->input_filename);
    fprintf(stdout, "  Dimensione: %d byte\n", compiler->stats.input_size);
    fprintf(stdout, "  Righe: %d\n", compiler->stats.input_lines);

    fprintf(stdout, "\nFile di output");
    if (compiler->output_filename)
    {
        fprintf(stdout, " (%s)", compiler->output_filename);
    }
    fprintf(stdout, ":\n");
    fprintf(stdout, "  Dimensione: %d byte\n", compiler->stats.output_size);
    fprintf(stdout, "  Righe: %d\n", compiler->stats.output_lines);

    fprintf(stdout, "\n===================================\n");
}