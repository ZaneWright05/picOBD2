#include <stdbool.h>
#include <stddef.h>

bool write_to_config(const char * msg);
bool find_in_config(const char* key, char* value, size_t value_size);
bool delete_from_config(const char* key);
bool update_config(const char* key, const char* new_value);
bool create_Dir(char * name);
void print_config();