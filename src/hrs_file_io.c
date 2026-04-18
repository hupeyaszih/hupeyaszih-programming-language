#include "hrs_file_io.h"
#include "globals.h"
#include <stddef.h>
#include <stdlib.h>

char *hrs_file_io_read_file(const char *restrict file_path){
    FILE *file_ptr;
    file_ptr = fopen(file_path, "rb");

    if(NULL == file_ptr){
        C_LOG_ERR("File IO failed to load \"%s\"", file_path);
        return NULL;
    }
    fseek(file_ptr, 0, SEEK_END);
    size_t length = ftell(file_ptr);

    fseek(file_ptr, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (buffer) {
        if(fread(buffer, 1, length, file_ptr) != length) {
            free(buffer);
            C_LOG_ERR("File IO failed to load \"%s\"", file_path);
            return NULL;
        }
        buffer[length] = '\0';
    }

    fclose(file_ptr);
    LOG_M_OK("FILE IO read \"%s\"", file_path);
    return buffer;
}
