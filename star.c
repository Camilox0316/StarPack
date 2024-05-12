#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define MAX_FILES 100       // Máximo de archivos que el sistema puede manejar
#define MAX_BLOCKS 1000     // Máximo de bloques que el sistema puede manejar
#define BLOCK_SIZE 262144   // Tamaño de cada bloque de datos (256K)

typedef struct {
    char filename[100];      // Nombre del archivo, con un máximo de 100 caracteres
    int start_block;         // Índice del primer bloque en el archivo empaquetado
    int num_blocks;          // Número de bloques que ocupa el archivo
    int file_size;           // Tamaño total del archivo en bytes
} FileMetadata;

typedef struct {
    FileMetadata files[MAX_FILES];       // Array de metadatos de archivo
    int free_blocks[MAX_BLOCKS];         // Lista de estados de bloques (0 libre, 1 ocupado)
    int next_free_block;                 // Índice del primer bloque libre
} FAT;

typedef struct {
    char *output_filename;
    char **input_filenames;
    int num_files;
} CreateArgs;


void initialize_fat(FAT *fat) {
    memset(fat, 0, sizeof(FAT));
    for (int i = 0; i < MAX_BLOCKS; i++) {
        fat->free_blocks[i] = 0;
    }
    fat->next_free_block = 0;
}

int assign_blocks(FAT *fat, int num_blocks_needed) {
    int block_count = 0;
    int start_block = -1;
    for (int i = fat->next_free_block; i < MAX_BLOCKS && block_count < num_blocks_needed; i++) {
        if (fat->free_blocks[i] == 0) {
            if (start_block == -1) start_block = i;
            block_count++;
        } else {
            start_block = -1;
            block_count = 0;
        }
    }
    if (block_count == num_blocks_needed) {
        for (int i = start_block; i < start_block + num_blocks_needed; i++) {
            fat->free_blocks[i] = 1;
        }
        fat->next_free_block = start_block + num_blocks_needed;
        return start_block;
    }
    return -1;
}

void create_archive(char *output_filename, int num_files, char *input_filenames[]) {
    FAT fat;
    initialize_fat(&fat);

    FILE *output_file = fopen(output_filename, "wb+");
    if (!output_file) {
        perror("Failed to open output file");
        return;
    }

    for (int i = 0; i < num_files; i++) {
        FILE *input_file = fopen(input_filenames[i], "rb");
        if (!input_file) {
            fprintf(stderr, "Failed to open input file %s\n", input_filenames[i]);
            continue;
        }

        fseek(input_file, 0, SEEK_END);
        int file_size = ftell(input_file);
        fseek(input_file, 0, SEEK_SET);
        int num_blocks_needed = (file_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

        int start_block = assign_blocks(&fat, num_blocks_needed);
        if (start_block == -1) {
            fprintf(stderr, "Failed to allocate space for file %s\n", input_filenames[i]);
            fclose(input_file);
            continue;
        }

        fseek(output_file, start_block * BLOCK_SIZE, SEEK_SET);
        char buffer[BLOCK_SIZE];
        int bytes_read;
        while ((bytes_read = fread(buffer, 1, BLOCK_SIZE, input_file)) > 0) {
            fwrite(buffer, 1, bytes_read, output_file);
        }

        strcpy(fat.files[i].filename, input_filenames[i]);
        fat.files[i].start_block = start_block;
        fat.files[i].num_blocks = num_blocks_needed;
        fat.files[i].file_size = file_size;

        fclose(input_file);
    }

    fseek(output_file, 0, SEEK_SET);
    fwrite(&fat, sizeof(FAT), 1, output_file);

    fclose(output_file);
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
    if (argc < 3) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int opt;
    int verbose = 0;
    CreateArgs create_args = {0};

    static struct option long_options[] = {
        {"create", no_argument, 0, 'c'},
        {"extract", no_argument, 0, 'x'},
        {"list", no_argument, 0, 't'},
        {"delete", no_argument, 0, 'd'},
        {"update", no_argument, 0, 'u'},
        {"verbose", no_argument, 0, 'v'},
        {"file", required_argument, 0, 'f'},
        {"append", no_argument, 0, 'r'},
        {"pack", no_argument, 0, 'p'},
        {0, 0, 0, 0}
    };

    int create_flag = 0;

    while ((opt = getopt_long(argc, argv, "cxtduvrf:pa", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                create_flag = 1;
                break;
            case 'f':
                if (create_flag) {
                    create_args.output_filename = optarg;
                    create_args.num_files = argc - optind;
                    create_args.input_filenames = &argv[optind];
                }
                break;
            case 'v':
                verbose = 1;
                printf("Modo verboso activado.\n");
                break;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (create_flag && create_args.output_filename) {
        if (create_args.num_files > 0) {
            printf("Creando archivo: %s con %d archivos de entrada.\n", create_args.output_filename, create_args.num_files);
            create_archive(create_args.output_filename, create_args.num_files, create_args.input_filenames);
        } else {
            fprintf(stderr, "No se especificaron archivos de entrada para la creación.\n");
            return EXIT_FAILURE;
        }
    }
 
    return 0;
}
