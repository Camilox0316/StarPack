#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#define KILOBYTES 256 // 256 KB
#define BLOCK_SIZE KILOBYTES * 1024 
#define MAX_ENTRIES 100 
#define MAX_NAME_LENGTH 256
#define MAX_BLOCKS_PER_ENTRY 64 
#define TOTAL_BLOCKS MAX_BLOCKS_PER_ENTRY * MAX_ENTRIES

typedef struct {
    char name[MAX_NAME_LENGTH];
    size_t size;
    size_t block_indices[MAX_BLOCKS_PER_ENTRY];
    size_t block_count; 
} Entry;

typedef struct {
    Entry entries[MAX_ENTRIES];
    size_t entry_count;
    size_t free_block_indices[TOTAL_BLOCKS];
    size_t free_block_count;
} FileAllocationTable;

typedef struct {
    unsigned char content[BLOCK_SIZE];
} DataBlock;


size_t locate_empty_block(FileAllocationTable *fat) {
    for (size_t i = 0; i < fat->free_block_count; i++) {
        if (fat->free_block_indices[i] != 0) {
            size_t free_block = fat->free_block_indices[i];
            fat->free_block_indices[i] = 0;
            return free_block;
        }
    }
    return (size_t)-1;
}

void enlarge_archive(FILE *archive, FileAllocationTable *fat) {
    fseek(archive, 0, SEEK_END);
    size_t current_size = ftell(archive);
    size_t expanded_size = current_size + BLOCK_SIZE;
    ftruncate(fileno(archive), expanded_size);
    fat->free_block_indices[fat->free_block_count++] = current_size;
}

void print_archive_files(const char *archive_name, bool verbose) {
    FILE *archive = fopen(archive_name, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado.\n");
        return;
    }

    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    printf("Contenido del archivo empacado:\n");
    printf("%-20s %-10s %s\n", "Nombre del archivo", "Tamaño", "Bloques");
    printf("%-20s %-10s %s\n", "-------------------", "----------", "------");

    for (size_t i = 0; i < fat.entry_count; i++) {
        Entry entry = fat.entries[i];
        printf("%-20s %-10zu ", entry.name, entry.size);

        if (verbose) {
            printf("  [");
            for (size_t j = 0; j < entry.block_count; j++) {
                printf("%zu", entry.block_indices[j]);
                if (j < entry.block_count - 1) {
                    printf(", ");
                }
            }
            printf("]");
        }
        printf("\n");
    }

    fclose(archive);
}

void save_data_block(FILE *archive, DataBlock *block, size_t position) {
    fseek(archive, position, SEEK_SET);
    fwrite(block, sizeof(DataBlock), 1, archive);
}

void refresh_file_table(FileAllocationTable *fat, const char *filename, size_t file_size, size_t block_position, size_t bytes_read) {
    for (size_t i = 0; i < fat->entry_count; i++) {
        if (strcmp(fat->entries[i].name, filename) == 0) {
            fat->entries[i].block_indices[fat->entries[i].block_count++] = block_position;
            fat->entries[i].size += bytes_read;
            return;
        }
    }

    Entry new_entry;
    strncpy(new_entry.name, filename, MAX_NAME_LENGTH);
    new_entry.size = file_size + bytes_read;
    new_entry.block_indices[0] = block_position;
    new_entry.block_count = 1;
    fat->entries[fat->entry_count++] = new_entry;
}

void save_file_table(FILE *archive, FileAllocationTable *fat) {
    fseek(archive, 0, SEEK_SET);
    fwrite(fat, sizeof(FileAllocationTable), 1, archive);
}

void build_archive(bool verbose, bool debug, const char *outputFile, bool file, char *inputFiles[], int numInputFiles) {
    if (verbose) printf("Creando el archivo empaquetado: %s\n", outputFile);
    FILE *archive = fopen(outputFile, "wb");

    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s'.\n", outputFile);
        exit(1);
    }

    // Inicializar la estructura FileAllocationTable
    FileAllocationTable fat;
    memset(&fat, 0, sizeof(FileAllocationTable));

    fat.free_block_indices[0] = sizeof(FileAllocationTable);
    fat.free_block_count = 1;

    fwrite(&fat, sizeof(FileAllocationTable), 1, archive);

    if (file && numInputFiles > 0) {
        for (int i = 0; i < numInputFiles; i++) {
            FILE *input_file = fopen(inputFiles[i], "rb");
            if (input_file == NULL) {
                fprintf(stderr, "Error: No se pudo abrir el archivo: '%s'.\n", inputFiles[i]);
                exit(1);
            }

            if (verbose) printf("\n------------------------------\n");
            if (verbose) printf("Agregando el archivo: '%s'\n", inputFiles[i]);

            size_t file_size = 0;
            size_t block_count = 0;
            DataBlock block;
            size_t bytes_read;

            while ((bytes_read = fread(&block, 1, sizeof(DataBlock), input_file)) > 0) {
                size_t block_position = locate_empty_block(&fat);
                if (block_position == (size_t)-1) {
                    if (debug) {
                        printf("Info: Expandiendo el archivo empaquetado por falta de bloques libres.\n");
                    }
                    enlarge_archive(archive, &fat);
                    block_position = locate_empty_block(&fat);
                    if (debug) {
                        printf("Info: Nuevo bloque libre encontrado en la posición %zu.\n", block_position);
                    }
                }

                if (bytes_read < sizeof(DataBlock)) {
                    memset((char*)&block + bytes_read, 0, sizeof(DataBlock) - bytes_read);
                }

                save_data_block(archive, &block, block_position);
                refresh_file_table(&fat, inputFiles[i], file_size, block_position, bytes_read);

                file_size += bytes_read;
                block_count++;

                if (debug) {
                    printf("Info: Escribiendo bloque %zu del archivo '%s' en la posición %zu.\n", block_count, inputFiles[i], block_position);
                }
            }

            if (verbose) printf("Tamaño final del archivo '%s': %zu bytes.\n", inputFiles[i], file_size);
            if (verbose) printf("------------------------------\n");

            fclose(input_file);
        }
    } else {
        if (verbose) {
            printf("Leyendo datos desde la entrada estándar (stdin)...\n");
        }

        size_t file_size = 0;
        size_t block_count = 0;
        DataBlock block;
        size_t bytes_read;
        while ((bytes_read = fread(&block, 1, sizeof(DataBlock), stdin)) > 0) {
            size_t block_position = locate_empty_block(&fat);
            if (block_position == (size_t)-1) {
                enlarge_archive(archive, &fat);
                block_position = locate_empty_block(&fat);
            }

            if (bytes_read < sizeof(DataBlock)) {
                memset((char*)&block + bytes_read, 0, sizeof(DataBlock) - bytes_read);
            }

            save_data_block(archive, &block, block_position);
            refresh_file_table(&fat, "stdin", file_size, block_position, bytes_read);

            file_size += bytes_read;
            block_count++;

            if (debug) {
                printf("Info: Escribiendo bloque %zu desde stdin en la posición %zu.\n", block_count, block_position);
            }
        }
    }

    // Escribir la estructura FileAllocationTable actualizada en el archivo
    save_file_table(archive, &fat);
    fclose(archive);
}

void remove_files_from_archive(const char *archive_name, char **filenames, int num_files, bool verbose, bool debug) {
    FILE *archive = fopen(archive_name, "rb+");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s' para modificación.\n", archive_name);
        return;
    }

    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    for (int i = 0; i < num_files; i++) {
        const char *filename = filenames[i];
        bool file_found = false;

        for (size_t j = 0; j < fat.entry_count; j++) {
            if (strcmp(fat.entries[j].name, filename) == 0) {
                file_found = true;

                // Marcar los bloques como libres
                for (size_t k = 0; k < fat.entries[j].block_count; k++) {
                    fat.free_block_indices[fat.free_block_count++] = fat.entries[j].block_indices[k];
                    if (debug) {
                        printf("Info: Bloque %zu del archivo '%s' marcado como libre.\n", fat.entries[j].block_indices[k], filename);
                    }
                }

                // Eliminar la entrada del archivo del FAT
                for (size_t k = j; k < fat.entry_count - 1; k++) {
                    fat.entries[k] = fat.entries[k + 1];
                }
                fat.entry_count--;

                if (verbose) {
                    printf("Info: Archivo '%s' eliminado del archivo empaquetado '%s'.\n", filename, archive_name);
                }

                break;
            }
        }

        if (!file_found) {
            fprintf(stderr, "Error: Archivo '%s' no encontrado en el archivo empaquetado '%s'.\n", filename, archive_name);
        }
    }

    // Escribir la estructura FileAllocationTable actualizada en el archivo
    fseek(archive, 0, SEEK_SET);
    fwrite(&fat, sizeof(FileAllocationTable), 1, archive);

    fclose(archive);
}


void retrieve_archive(const char *archive_name, bool verbose, bool debug) {
    FILE *archive = fopen(archive_name, "rb");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s' para lectura.\n", archive_name);
        return;
    }

    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    for (size_t i = 0; i < fat.entry_count; i++) {
        Entry entry = fat.entries[i];
        FILE *output_file = fopen(entry.name, "wb");
        if (output_file == NULL) {
            fprintf(stderr, "Error: No se pudo crear el archivo de salida '%s'.\n", entry.name);
            continue;
        }

        if (verbose) {
            printf("Extrayendo archivo: '%s'\n", entry.name);
        }

        size_t file_size = 0;
        for (size_t j = 0; j < entry.block_count; j++) {
            DataBlock block;
            fseek(archive, entry.block_indices[j], SEEK_SET);
            fread(&block, sizeof(DataBlock), 1, archive);

            size_t bytes_to_write = (file_size + sizeof(DataBlock) > entry.size) ? entry.size - file_size : sizeof(DataBlock);
            fwrite(&block, 1, bytes_to_write, output_file);

            file_size += bytes_to_write;

            if (debug) {
                printf("Info: Bloque %zu del archivo '%s' extraído de la posición %zu.\n", j + 1, entry.name, entry.block_indices[j]);
            }
        }

        fclose(output_file);
    }

    fclose(archive);
}


void modify_files_in_archive(const char *archive_name, char **filenames, int num_files, bool verbose, bool debug) {
    FILE *archive = fopen(archive_name, "rb+");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s' para modificación.\n", archive_name);
        return;
    }

    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    for (int i = 0; i < num_files; i++) {
        const char *filename = filenames[i];
        bool file_found = false;

        for (size_t j = 0; j < fat.entry_count; j++) {
            if (strcmp(fat.entries[j].name, filename) == 0) {
                file_found = true;

                // Marcar los bloques anteriores como libres
                for (size_t k = 0; k < fat.entries[j].block_count; k++) {
                    fat.free_block_indices[fat.free_block_count++] = fat.entries[j].block_indices[k];
                    if (debug) {
                        printf("Info: El bloque %zu del archivo '%s' se ha marcado como libre.\n", fat.entries[j].block_indices[k], filename);
                    }
                }

                // Leer el contenido actualizado del archivo
                FILE *input_file = fopen(filename, "rb");
                if (input_file == NULL) {
                    fprintf(stderr, "Error: No se pudo abrir el archivo de entrada '%s'.\n", filename);
                    continue;
                }

                size_t file_size = 0;
                size_t block_count = 0;
                DataBlock block;
                size_t bytes_read;
                while ((bytes_read = fread(&block, 1, sizeof(DataBlock), input_file)) > 0) {
                    size_t block_position = locate_empty_block(&fat);
                    if (block_position == (size_t)-1) {
                        enlarge_archive(archive, &fat);
                        block_position = locate_empty_block(&fat);
                    }

                    save_data_block(archive, &block, block_position);
                    fat.entries[j].block_indices[block_count++] = block_position;

                    file_size += bytes_read;

                    if (debug) {
                        printf("Info: Bloque %zu del archivo '%s' actualizado en la posición %zu.\n", block_count, filename, block_position);
                    }
                }

                fat.entries[j].size = file_size;
                fat.entries[j].block_count = block_count;

                fclose(input_file);

                if (verbose) {
                    printf("Info: El archivo '%s' se ha actualizado en el archivo empaquetado '%s'.\n", filename, archive_name);
                }

                break;
            }
        }

        if (!file_found) {
            fprintf(stderr, "Error: El archivo '%s' no se encontró en el archivo empaquetado '%s'.\n", filename, archive_name);
        }
    }

    // Escribir la estructura FileAllocationTable actualizada en el archivo
    fseek(archive, 0, SEEK_SET);
    fwrite(&fat, sizeof(FileAllocationTable), 1, archive);

    fclose(archive);
}

void optimize_archive(const char *archive_name, bool verbose, bool debug) {
    // Abrir el archivo empaquetado para lectura y escritura
    FILE *archive = fopen(archive_name, "rb+");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s' para modificación.\n", archive_name);
        return;
    }

    // Leer la estructura FileAllocationTable del archivo
    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    size_t new_block_position = sizeof(FileAllocationTable);  // Nueva posición de inicio para los bloques
    for (size_t i = 0; i < fat.entry_count; i++) {
        Entry *entry = &fat.entries[i];

        for (size_t j = 0; j < entry->block_count; j++) {
            DataBlock block;
            // Leer el bloque actual desde su posición en el archivo
            fseek(archive, entry->block_indices[j], SEEK_SET);
            fread(&block, sizeof(DataBlock), 1, archive);

            // Escribir el bloque en la nueva posición
            fseek(archive, new_block_position, SEEK_SET);
            fwrite(&block, sizeof(DataBlock), 1, archive);

            // Actualizar la posición del bloque en la entrada del archivo
            entry->block_indices[j] = new_block_position;
            new_block_position += sizeof(DataBlock);

            if (debug) {
                printf("Info: El bloque %zu del archivo '%s' se ha movido a la posición %zu.\n", j + 1, entry->name, entry->block_indices[j]);
            }
        }

        if (verbose) {
            printf("Info: El archivo '%s' se ha desfragmentado.\n", entry->name);
        }
    }

    // Actualizar la estructura FileAllocationTable con los nuevos bloques libres
    fat.free_block_count = 0;    
    size_t remaining_space = new_block_position;
    while (remaining_space < fat.free_block_indices[fat.free_block_count - 1]) {
        fat.free_block_indices[fat.free_block_count++] = remaining_space;
        remaining_space += sizeof(DataBlock);
    }

    // Escribir la estructura FileAllocationTable actualizada en el archivo
    fseek(archive, 0, SEEK_SET);
    fwrite(&fat, sizeof(FileAllocationTable), 1, archive);

    // Truncar el archivo para eliminar el espacio no utilizado
    ftruncate(fileno(archive), new_block_position);

    fclose(archive);
}

void add_files_to_archive(const char *archive_name, char **filenames, int num_files, bool verbose, bool debug) {
    // Abrir el archivo empaquetado para lectura y escritura
    FILE *archive = fopen(archive_name, "rb+");
    if (archive == NULL) {
        fprintf(stderr, "Error: No se pudo abrir el archivo empaquetado '%s' para modificación.\n", archive_name);
        return; 
    }

    // Leer la estructura FileAllocationTable del archivo
    FileAllocationTable fat;
    fread(&fat, sizeof(FileAllocationTable), 1, archive);

    if (num_files == 0) {
        // Leer desde la entrada estándar (stdin)
        const char *filename = "stdin";
        size_t file_size = 0;
        size_t block_count = 0;
        DataBlock block;
        size_t bytes_read;
        while ((bytes_read = fread(&block, 1, sizeof(DataBlock), stdin)) > 0) {
            size_t block_position = locate_empty_block(&fat);
            if (block_position == (size_t)-1) {
                // Expandir el archivo si no hay bloques libres
                enlarge_archive(archive, &fat);
                block_position = locate_empty_block(&fat);
            }

            // Escribir el bloque en la nueva posición
            save_data_block(archive, &block, block_position);
            refresh_file_table(&fat, filename, file_size, block_position, bytes_read);

            file_size += bytes_read;
            block_count++;

            if (debug) {
                printf("Info: Bloque %zu leído desde stdin y agregado en la posición %zu.\n", block_count, block_position);
            }
        }

        if (verbose) {
            printf("Info: Contenido de stdin agregado al archivo empaquetado como '%s'.\n", filename);
        }   
    } else {
        // Agregar archivos especificados
        for (int i = 0; i < num_files; i++) {
            const char *filename = filenames[i];
            FILE *input_file = fopen(filename, "rb");
            if (input_file == NULL) {
                fprintf(stderr, "Error: No se pudo abrir el archivo de entrada '%s'.\n", filename);
                continue;
            }

            size_t file_size = 0;
            size_t block_count = 0;
            DataBlock block;
            size_t bytes_read;  
            while ((bytes_read = fread(&block, 1, sizeof(DataBlock), input_file)) > 0) {
                size_t block_position = locate_empty_block(&fat);
                if (block_position == (size_t)-1) {
                    // Expandir el archivo si no hay bloques libres
                    enlarge_archive(archive, &fat);
                    block_position = locate_empty_block(&fat);
                }

                // Escribir el bloque en la nueva posición
                save_data_block(archive, &block, block_position);
                refresh_file_table(&fat, filename, file_size, block_position, bytes_read);

                file_size += bytes_read;
                block_count++;

                if (debug) {
                    printf("Info: Bloque %zu del archivo '%s' agregado en la posición %zu.\n", block_count, filename, block_position);
                }
            }

            fclose(input_file);

            if (verbose) {
                printf("Info: Archivo '%s' agregado al archivo empaquetado.\n", filename);
            }
        }
    }

    // Escribir la estructura FileAllocationTable actualizada en el archivo
    fseek(archive, 0, SEEK_SET);
    fwrite(&fat, sizeof(FileAllocationTable), 1, archive);

    fclose(archive);
}

// Función para validar que el archivo tenga la extensión .tar
bool validate_tar_extension(const char *filename) {
    const char *tar_ext = ".tar";
    size_t len = strlen(filename);
    return len >= 4 && strcmp(filename + len - 4, tar_ext) == 0;
}

void print_usage(char *program_name) {
    printf("Uso: %s [opciones] [argumentos]\n", program_name);
    printf("Opciones:\n");
    printf("  -c, --create               Crea un nuevo archivo\n");
    printf("  -x, --extract              Extrae de un archivo\n");
    printf("  -t, --list                 Lista los contenidos de un archivo\n");
    printf("  --delete                   Borra desde un archivo\n");
    printf("  -u, --update               Actualiza el contenido del archivo\n");
    printf("  -v, --verbose              Muestra información detallada de las operaciones\n");
    printf("  -f, --file [archivo]       Especifica el archivo para operar\n");
    printf("  -r, --append               Agrega contenido a un archivo\n");
    printf("  -p, --pack                 Desfragmenta el contenido del archivo\n");
}

int main(int argc, char *argv[]) {
    bool create = false;
    bool extract = false;
    bool list = false;
    bool delete = false;
    bool update = false;
    bool verbose = false;
    bool debug = false;  
    bool file = false;
    bool append = false;
    bool pack = false;
    char *outputFile = NULL;
    char **inputFiles = NULL;
    int numInputFiles = 0;
    int opt;

    static struct option long_options[] = {
        {"create",      no_argument,       0, 'c'},
        {"extract",     no_argument,       0, 'x'},
        {"list",        no_argument,       0, 't'},
        {"delete",      no_argument,       0, 'd'},
        {"update",      no_argument,       0, 'u'},
        {"verbose",     no_argument,       0, 'v'},
        {"file",        no_argument,       0, 'f'},
        {"append",      no_argument,       0, 'r'},
        {"pack",        no_argument,       0, 'p'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "cxtduvfrp", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                create = true;
                break;
            case 'x':
                extract = true;
                break;
            case 't':
                list = true;
                break;
            case 'd':
                delete = true;
                break;
            case 'u':
                update = true;
                break;
            case 'v':
                if (verbose) {
                    debug = true;  
                }
                verbose = true;
                break;
            case 'f':
                file = true;
                break;
            case 'r':
                append = true;
                break;
            case 'p':
                pack = true;
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;                
        }
    }

    if (optind < argc) {
        outputFile = argv[optind++];

        // Validar la extensión .tar para las opciones que requieren un archivo .tar
        if (create || extract || list || delete || update || append || pack) {
            if (!validate_tar_extension(outputFile)) {
                fprintf(stderr, "Error: El archivo de salida debe tener la extensión .tar\n");
                return 1;
            }
        }
    } else {
        if (create || extract || list || delete || update || append || pack) {
            fprintf(stderr, "Error: Se debe especificar un archivo de salida con la extensión .tar\n");
            return 1;
        }
    }

    numInputFiles = argc - optind;
    if (numInputFiles > 0) {
        inputFiles = &argv[optind];
    }

    if (create) {
        build_archive(verbose, debug, outputFile, file, inputFiles, numInputFiles);
    } else if (extract) {
        retrieve_archive(outputFile, verbose, debug);
    } else if (delete) {
        remove_files_from_archive(outputFile, inputFiles, numInputFiles, verbose, debug);
    } else if (update) {
        modify_files_in_archive(outputFile, inputFiles, numInputFiles, verbose, debug);
    } else if (append) {
        add_files_to_archive(outputFile, inputFiles, numInputFiles, verbose, debug);
    }

    if (pack) {
        optimize_archive(outputFile, verbose, debug);
    }
    if (list) {
        print_archive_files(outputFile, verbose);
    }

    return 0;
}
