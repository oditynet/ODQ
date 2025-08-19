#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <dirent.h>
#include <termios.h>
#include <unistd.h>

#define MAX_TABLE_NAME 50
#define MAX_FIELD_NAME 30
#define MAX_FIELDS 20
#define MAX_RECORD_SIZE 4096
#define MAX_QUERY_LENGTH 1024
#define TABLE_PREFIX "ODQ"
#define HISTORY_SIZE 30
void process_command(const char* command);
// Структуры данных
typedef enum { FIELD_INT, FIELD_TEXT, FIELD_BOOL } FieldType;

typedef struct {
    char name[MAX_FIELD_NAME];
    FieldType type;
    int size;
} Field;

typedef struct AVLNode {
    char key[256];
    long file_position;
    struct AVLNode* left;
    struct AVLNode* right;
    int height;
} AVLNode;

typedef struct {
    char name[MAX_TABLE_NAME];
    char filename[100];
    Field fields[MAX_FIELDS];
    int field_count;
    int record_size;
    AVLNode* indexes[MAX_FIELDS];
    int auto_increment;
    FILE* data_file;
} Table;

// Глобальные переменные
Table current_table;
bool table_loaded = false;
char command_history[HISTORY_SIZE][MAX_QUERY_LENGTH];
int history_count = 0;
int history_current = -1;
struct termios orig_termios;

// Функции для работы с терминалом
void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Функции истории команд
void add_to_history(const char* command) {
    if (strlen(command) == 0) return;
    
    // Не добавляем повторные команды
    if (history_count > 0 && strcmp(command_history[history_count-1], command) == 0) {
        return;
    }
    
    if (history_count < HISTORY_SIZE) {
        strcpy(command_history[history_count], command);
        history_count++;
    } else {
        for (int i = 1; i < HISTORY_SIZE; i++) {
            strcpy(command_history[i-1], command_history[i]);
        }
        strcpy(command_history[HISTORY_SIZE-1], command);
    }
    history_current = history_count;
}

const char* get_history_command(int direction) {
    if (history_count == 0) return "";
    
    if (direction == 1) { // Вверх
        if (history_current > 0) history_current--;
    } else { // Вниз
        if (history_current < history_count - 1) history_current++;
    }
    
    return (history_current >= 0 && history_current < history_count) ? 
           command_history[history_current] : "";
}

// AVL functions
int height(AVLNode* node) {
    return node ? node->height : 0;
}

int max(int a, int b) {
    return (a > b) ? a : b;
}

AVLNode* newAVLNode(const char* key, long position) {
    AVLNode* node = (AVLNode*)malloc(sizeof(AVLNode));
    strncpy(node->key, key, sizeof(node->key));
    node->file_position = position;
    node->left = node->right = NULL;
    node->height = 1;
    return node;
}

AVLNode* rightRotate(AVLNode* y) {
    AVLNode* x = y->left;
    AVLNode* T2 = x->right;
    x->right = y;
    y->left = T2;
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;
    return x;
}

AVLNode* leftRotate(AVLNode* x) {
    AVLNode* y = x->right;
    AVLNode* T2 = y->left;
    y->left = x;
    x->right = T2;
    x->height = max(height(x->left), height(x->right)) + 1;
    y->height = max(height(y->left), height(y->right)) + 1;
    return y;
}

int getBalance(AVLNode* node) {
    return node ? height(node->left) - height(node->right) : 0;
}

AVLNode* insertAVL(AVLNode* node, const char* key, long position) {
    if (!node) return newAVLNode(key, position);
    
    int cmp = strcmp(key, node->key);
    if (cmp < 0) node->left = insertAVL(node->left, key, position);
    else if (cmp > 0) node->right = insertAVL(node->right, key, position);
    else return node;
    
    node->height = 1 + max(height(node->left), height(node->right));
    int balance = getBalance(node);
    
    if (balance > 1 && strcmp(key, node->left->key) < 0)
        return rightRotate(node);
    if (balance < -1 && strcmp(key, node->right->key) > 0)
        return leftRotate(node);
    if (balance > 1 && strcmp(key, node->left->key) > 0) {
        node->left = leftRotate(node->left);
        return rightRotate(node);
    }
    if (balance < -1 && strcmp(key, node->right->key) < 0) {
        node->right = rightRotate(node->right);
        return leftRotate(node);
    }
    return node;
}

AVLNode* searchAVL(AVLNode* root, const char* key) {
    if (!root || strcmp(root->key, key) == 0) return root;
    if (strcmp(key, root->key) < 0) return searchAVL(root->left, key);
    return searchAVL(root->right, key);
}

void searchTextInAVL(AVLNode* root, const char* search_text, long** results, int* count) {
    if (!root) return;
    if (strstr(root->key, search_text) != NULL) {
        *results = realloc(*results, (*count + 1) * sizeof(long));
        (*results)[*count] = root->file_position;
        (*count)++;
    }
    searchTextInAVL(root->left, search_text, results, count);
    searchTextInAVL(root->right, search_text, results, count);
}

// Table functions
void create_table(const char* table_name, const char* field_definitions) {
    char filename[100];
    snprintf(filename, sizeof(filename), "%s_%s.bin", TABLE_PREFIX, table_name);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        printf("Error creating table\n");
        return;
    }
    
    Table table;
    strcpy(table.name, table_name);
    strcpy(table.filename, filename);
    table.field_count = 0;
    table.record_size = 0;
    table.auto_increment = 1;
    table.data_file = NULL;
    for (int i = 0; i < MAX_FIELDS; i++) table.indexes[i] = NULL;
    
    char def_copy[MAX_QUERY_LENGTH];
    strcpy(def_copy, field_definitions);
    char* token = strtok(def_copy, ",");
    
    while (token && table.field_count < MAX_FIELDS) {
        while (*token == ' ') token++;
        
        char field_name[MAX_FIELD_NAME];
        char field_type[20];
        if (sscanf(token, "%s %s", field_name, field_type) == 2) {
            Field field;
            strcpy(field.name, field_name);
            
            if (strcmp(field_type, "int") == 0) {
                field.type = FIELD_INT;
                field.size = sizeof(int);
            } else if (strncmp(field_type, "text", 4) == 0) {
                field.type = FIELD_TEXT;
                field.size = 255;
                char* bracket = strchr(token, '(');
                if (bracket && sscanf(bracket, "(%d)", &field.size) == 1) {}
            } else if (strcmp(field_type, "bool") == 0) {
                field.type = FIELD_BOOL;
                field.size = sizeof(bool);
            } else {
                printf("Unknown field type: %s\n", field_type);
                fclose(file);
                return;
            }
            
            table.record_size += field.size;
            table.fields[table.field_count++] = field;
        }
        token = strtok(NULL, ",");
    }
    
    fwrite(&table, sizeof(Table), 1, file);
    fclose(file);
    printf("Table '%s' created\n", table_name);
}

bool load_table(const char* table_name) {
    char filename[100];
    snprintf(filename, sizeof(filename), "%s_%s.bin", TABLE_PREFIX, table_name);
    
    FILE* file = fopen(filename, "rb+");
    if (!file) {
        printf("Table '%s' not found\n", table_name);
        return false;
    }
    
    if (fread(&current_table, sizeof(Table), 1, file) != 1) {
        fclose(file);
        printf("Error reading table\n");
        return false;
    }
    
    current_table.data_file = file;
    
    // Rebuild indexes from data
    fseek(file, sizeof(Table), SEEK_SET);
    char* record = malloc(current_table.record_size);
    long position = ftell(file);
    
    while (fread(record, current_table.record_size, 1, file)) {
        int offset = 0;
        for (int i = 0; i < current_table.field_count; i++) {
            Field field = current_table.fields[i];
            char key[256] = {0};
            
            switch (field.type) {
                case FIELD_INT: {
                    int value;
                    memcpy(&value, record + offset, sizeof(int));
                    snprintf(key, sizeof(key), "%d", value);
                    break;
                }
                case FIELD_TEXT:
                    strncpy(key, record + offset, field.size);
                    key[field.size] = '\0';
                    break;
                case FIELD_BOOL: {
                    bool value;
                    memcpy(&value, record + offset, sizeof(bool));
                    strcpy(key, value ? "true" : "false");
                    break;
                }
            }
            
            current_table.indexes[i] = insertAVL(current_table.indexes[i], key, position);
            offset += field.size;
        }
        position = ftell(file);
    }
    free(record);
    
    table_loaded = true;
    printf("Table '%s' loaded with indexes\n", table_name);
    return true;
}

void insert_into_table(const char* values) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    char values_copy[MAX_QUERY_LENGTH];
    strcpy(values_copy, values);
    char* token = strtok(values_copy, ",");
    
    char record[MAX_RECORD_SIZE] = {0};
    int offset = 0;
    
    for (int i = 0; i < current_table.field_count && token; i++) {
        while (*token == ' ' || *token == '\'') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\'')) *end-- = '\0';
        
        Field field = current_table.fields[i];
        
        switch (field.type) {
            case FIELD_INT: {
                int value = atoi(token);
                memcpy(record + offset, &value, sizeof(int));
                break;
            }
            case FIELD_TEXT:
                strncpy(record + offset, token, field.size);
                break;
            case FIELD_BOOL: {
                bool value = (strcasecmp(token, "true") == 0 || strcmp(token, "1") == 0);
                memcpy(record + offset, &value, sizeof(bool));
                break;
            }
        }
        offset += field.size;
        token = strtok(NULL, ",");
    }
    
    fseek(current_table.data_file, 0, SEEK_END);
    long position = ftell(current_table.data_file);
    fwrite(record, current_table.record_size, 1, current_table.data_file);
    fflush(current_table.data_file);
    
    // Update indexes
    offset = 0;
    for (int i = 0; i < current_table.field_count; i++) {
        Field field = current_table.fields[i];
        char key[256] = {0};
        
        switch (field.type) {
            case FIELD_INT: {
                int value;
                memcpy(&value, record + offset, sizeof(int));
                snprintf(key, sizeof(key), "%d", value);
                break;
            }
            case FIELD_TEXT:
                strncpy(key, record + offset, field.size);
                key[field.size] = '\0';
                break;
            case FIELD_BOOL: {
                bool value;
                memcpy(&value, record + offset, sizeof(bool));
                strcpy(key, value ? "true" : "false");
                break;
            }
        }
        
        current_table.indexes[i] = insertAVL(current_table.indexes[i], key, position);
        offset += field.size;
    }
    
    //printf("1 row inserted\n");
}

void select_all() {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        int offset = 0;
        for (int i = 0; i < current_table.field_count; i++) {
            Field field = current_table.fields[i];
            
            switch (field.type) {
                case FIELD_INT: {
                    int value;
                    memcpy(&value, record + offset, sizeof(int));
                    printf("%s: %d", field.name, value);
                    break;
                }
                case FIELD_TEXT: {
                    char text[field.size + 1];
                    strncpy(text, record + offset, field.size);
                    text[field.size] = '\0';
                    printf("%s: '%s'", field.name, text);
                    break;
                }
                case FIELD_BOOL: {
                    bool value;
                    memcpy(&value, record + offset, sizeof(bool));
                    printf("%s: %s", field.name, value ? "true" : "false");
                    break;
                }
            }
            offset += field.size;
            if (i < current_table.field_count - 1) printf(" | ");
        }
        printf("\n");
        count++;
    }
    
    printf("%d rows returned\n", count);
}

void select_field(const char* field_name) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    int field_index = -1;
    for (int i = 0; i < current_table.field_count; i++) {
        if (strcmp(current_table.fields[i].name, field_name) == 0) {
            field_index = i;
            break;
        }
    }
    
    if (field_index == -1) {
        printf("Field '%s' not found\n", field_name);
        return;
    }
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        int offset = 0;
        for (int i = 0; i < field_index; i++) {
            offset += current_table.fields[i].size;
        }
        
        Field field = current_table.fields[field_index];
        
        switch (field.type) {
            case FIELD_INT: {
                int value;
                memcpy(&value, record + offset, sizeof(int));
                printf("%s: %d\n", field.name, value);
                break;
            }
            case FIELD_TEXT: {
                char text[field.size + 1];
                strncpy(text, record + offset, field.size);
                text[field.size] = '\0';
                printf("%s: '%s'\n", field.name, text);
                break;
            }
            case FIELD_BOOL: {
                bool value;
                memcpy(&value, record + offset, sizeof(bool));
                printf("%s: %s\n", field.name, value ? "true" : "false");
                break;
            }
        }
        count++;
    }
    
    printf("%d rows returned\n", count);
}

bool compare_values(const char* field_value, const char* operator, const char* compare_value, FieldType field_type) {
    if (strcmp(operator, "=") == 0 || strcmp(operator, "==") == 0) {
        return strcmp(field_value, compare_value) == 0;
    }
    else if (strcmp(operator, "!=") == 0) {
        return strcmp(field_value, compare_value) != 0;
    }
    
    // Для числовых сравнений
    if (field_type == FIELD_INT || field_type == FIELD_BOOL) {
        int field_val = atoi(field_value);
        int cmp_val = atoi(compare_value);
        
        if (strcmp(operator, ">") == 0) {
            return field_val > cmp_val;
        }
        else if (strcmp(operator, "<") == 0) {
            return field_val < cmp_val;
        }
        else if (strcmp(operator, ">=") == 0) {
            return field_val >= cmp_val;
        }
        else if (strcmp(operator, "<=") == 0) {
            return field_val <= cmp_val;
        }
    }
    
    return false;
}

void select_where(const char* field_name, const char* operator, const char* value) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    int field_index = -1;
    for (int i = 0; i < current_table.field_count; i++) {
        if (strcmp(current_table.fields[i].name, field_name) == 0) {
            field_index = i;
            break;
        }
    }
    
    if (field_index == -1) {
        printf("Field '%s' not found\n", field_name);
        return;
    }
    
    Field field = current_table.fields[field_index];
    
    // Убираем кавычки из значения, если они есть
    char clean_value[100];
    strcpy(clean_value, value);
    if (clean_value[0] == '\'' && clean_value[strlen(clean_value)-1] == '\'') {
        memmove(clean_value, clean_value + 1, strlen(clean_value) - 2);
        clean_value[strlen(clean_value) - 2] = '\0';
    }
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        int offset = 0;
        for (int i = 0; i < field_index; i++) {
            offset += current_table.fields[i].size;
        }
        
        char field_value[256] = {0};
        
        switch (field.type) {
            case FIELD_INT: {
                int val;
                memcpy(&val, record + offset, sizeof(int));
                snprintf(field_value, sizeof(field_value), "%d", val);
                break;
            }
            case FIELD_TEXT:
                strncpy(field_value, record + offset, field.size);
                field_value[field.size] = '\0';
                break;
            case FIELD_BOOL: {
                bool val;
                memcpy(&val, record + offset, sizeof(bool));
                strcpy(field_value, val ? "true" : "false");
                break;
            }
        }
        
        if (compare_values(field_value, operator, clean_value, field.type)) {
            int print_offset = 0;
            for (int i = 0; i < current_table.field_count; i++) {
                Field f = current_table.fields[i];
                
                switch (f.type) {
                    case FIELD_INT: {
                        int val;
                        memcpy(&val, record + print_offset, sizeof(int));
                        printf("%s: %d", f.name, val);
                        break;
                    }
                    case FIELD_TEXT: {
                        char text[f.size + 1];
                        strncpy(text, record + print_offset, f.size);
                        text[f.size] = '\0';
                        printf("%s: '%s'", f.name, text);
                        break;
                    }
                    case FIELD_BOOL: {
                        bool val;
                        memcpy(&val, record + print_offset, sizeof(bool));
                        printf("%s: %s", f.name, val ? "true" : "false");
                        break;
                    }
                }
                print_offset += f.size;
                if (i < current_table.field_count - 1) printf(" | ");
            }
            printf("\n");
            count++;
        }
    }
    
    printf("%d rows returned\n", count);
}

void find_text(const char* search_text) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    printf("Searching for text: '%s'\n", search_text);
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        bool found = false;
        int offset = 0;
        
        for (int i = 0; i < current_table.field_count; i++) {
            Field field = current_table.fields[i];
            
            if (field.type == FIELD_TEXT) {
                char text[field.size + 1];
                strncpy(text, record + offset, field.size);
                text[field.size] = '\0';
                
                if (strstr(text, search_text) != NULL) {
                    found = true;
                    break;
                }
            }
            offset += field.size;
        }
        
        if (found) {
            offset = 0;
            for (int i = 0; i < current_table.field_count; i++) {
                Field field = current_table.fields[i];
                
                switch (field.type) {
                    case FIELD_INT: {
                        int value;
                        memcpy(&value, record + offset, sizeof(int));
                        printf("%s: %d", field.name, value);
                        break;
                    }
                    case FIELD_TEXT: {
                        char text[field.size + 1];
                        strncpy(text, record + offset, field.size);
                        text[field.size] = '\0';
                        printf("%s: '%s'", field.name, text);
                        break;
                    }
                    case FIELD_BOOL: {
                        bool value;
                        memcpy(&value, record + offset, sizeof(bool));
                        printf("%s: %s", field.name, value ? "true" : "false");
                        break;
                    }
                }
                offset += field.size;
                if (i < current_table.field_count - 1) printf(" | ");
            }
            printf("\n");
            count++;
        }
    }
    
    if (count == 0) {
        printf("No records found with text: '%s'\n", search_text);
    } else {
        printf("%d records found\n", count);
    }
}

void load_macro(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open file: %s\n", filename);
        return;
    }
    
    printf("Loading macro from %s:\n", filename);
    char line[MAX_QUERY_LENGTH];
    int line_num = 1;
    
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0 && line[0] != '#') {
            //printf("\n[Line %d] %s\n", line_num, line);
            process_command(line);
        }
        line_num++;
    }
    
    fclose(file);
    printf("Macro execution completed\n");
}

void process_command(const char* command) {
    char cmd[20], rest[MAX_QUERY_LENGTH] = {0};
    sscanf(command, "%19s %[^\n]", cmd, rest);
    
    for (char* p = cmd; *p; p++) *p = toupper(*p);
    
    if (strcmp(cmd, "CREATE") == 0) {
        char table_name[50], fields[500];
        if (sscanf(rest, "TABLE %s (%[^)]", table_name, fields) == 2) {
            create_table(table_name, fields);
        } else {
            printf("Syntax: CREATE TABLE name (field1 type, field2 type, ...)\n");
        }
    }
    else if (strcmp(cmd, "USE") == 0) {
        char table_name[50];
        if (sscanf(rest, "%s", table_name) == 1) {
            load_table(table_name);
        } else {
            printf("Syntax: USE tablename\n");
        }
    }
    else if (strcmp(cmd, "INSERT") == 0) {
        char values[500];
        if (sscanf(rest, "INTO %*s VALUES (%[^)]", values) == 1) {
            insert_into_table(values);
        } else {
            printf("Syntax: INSERT INTO tablename VALUES (value1, value2, ...)\n");
        }
    }
    else if (strcmp(cmd, "SELECT") == 0) {
        if (strstr(rest, "WHERE") != NULL) {
            char field_name[50], operator[10], value[100];
            
            // Улучшенный парсинг WHERE
            char* where_pos = strstr(rest, "WHERE");
            if (where_pos) {
                char where_part[200];
                strcpy(where_part, where_pos + 6); // "WHERE " = 6 символов
                
                // Убираем начальные пробелы
                char* start = where_part;
                while (*start == ' ') start++;
                
                // Ищем поле
                char* space1 = strchr(start, ' ');
                if (space1) {
                    *space1 = '\0';
                    strcpy(field_name, start);
                    
                    // Ищем оператор
                    char* op_start = space1 + 1;
                    while (*op_start == ' ') op_start++;
                    
                    char* space2 = strchr(op_start, ' ');
                    if (space2) {
                        *space2 = '\0';
                        strcpy(operator, op_start);
                        
                        // Ищем значение
                        char* val_start = space2 + 1;
                        while (*val_start == ' ') val_start++;
                        
                        // Обрабатываем кавычки
                        if (*val_start == '\'') {
                            char* end_quote = strchr(val_start + 1, '\'');
                            if (end_quote) {
                                *end_quote = '\0';
                                strcpy(value, val_start + 1);
                            } else {
                                strcpy(value, val_start);
                            }
                        } else {
                            // Без кавычек - берем до конца строки
                            strcpy(value, val_start);
                            // Убираем возможные пробелы в конце
                            char* end = value + strlen(value) - 1;
                            while (end > value && *end == ' ') *end-- = '\0';
                        }
                        
                        select_where(field_name, operator, value);
                    } else {
                        printf("Syntax error: missing operator or value\n");
                    }
                } else {
                    printf("Syntax: SELECT * FROM tablename WHERE field operator value\n");
                }
            }
        } else if (strstr(rest, "*") != NULL) {
            select_all();
        } else {
            char field_name[50];
            if (sscanf(rest, "%s FROM", field_name) == 1) {
                select_field(field_name);
            } else {
                printf("Syntax: SELECT field FROM tablename\n");
            }
        }
    }
    else if (strcmp(cmd, "FIND") == 0) {
        char text[100];
        if (sscanf(rest, "TEXT '%[^']", text) == 1) {
            find_text(text);
        } else {
            printf("Syntax: FIND TEXT 'searchtext'\n");
        }
    }
    else if (strcmp(cmd, "LOAD") == 0) {
        char filename[100];
        if (sscanf(rest, "%s", filename) == 1) {
            load_macro(filename);
        } else {
            printf("Syntax: LOAD filename\n");
        }
    }
    else if (strcmp(cmd, "EXIT") == 0) {
        exit(0);
    }
    else if (strcmp(cmd, "HELP") == 0) {
        printf("Available commands:\n");
        printf("  CREATE TABLE name (field1 type, field2 type, ...)\n");
        printf("  USE tablename\n");
        printf("  INSERT INTO tablename VALUES (value1, value2, ...)\n");
        printf("  SELECT * FROM tablename\n");
        printf("  SELECT field FROM tablename\n");
        printf("  SELECT * FROM tablename WHERE field operator value\n");
        printf("  FIND TEXT 'searchtext'\n");
        printf("  LOAD filename - Execute macro from file\n");
        printf("  EXIT\n");
        printf("\nWHERE operators: =, ==, !=, >, <, >=, <=\n");
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }
    
    add_to_history(command);
}

// Функция для чтения команды с поддержкой истории
char* read_command_with_history() {
    static char command[MAX_QUERY_LENGTH] = {0};
    int pos = 0;
    history_current = history_count;
    
    printf("ODQ> ");
    fflush(stdout);
    
    enable_raw_mode();
    
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == '\n') { // Enter
                printf("\n");
                command[pos] = '\0';
                disable_raw_mode();
                return command;
            }
            else if (c == 27) { // Escape sequence (стрелки)
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[0] == '[') {
                        if (seq[1] == 'A') { // Стрелка вверх
                            const char* prev_cmd = get_history_command(1);
                            // Очищаем строку
                            for (int i = 0; i < pos; i++) printf("\b \b");
                            strcpy(command, prev_cmd);
                            pos = strlen(command);
                            printf("%s", command);
                            fflush(stdout);
                        }
                        else if (seq[1] == 'B') { // Стрелка вниз
                            const char* next_cmd = get_history_command(0);
                            for (int i = 0; i < pos; i++) printf("\b \b");
                            strcpy(command, next_cmd);
                            pos = strlen(command);
                            printf("%s", command);
                            fflush(stdout);
                        }
                    }
                }
            }
            else if (c == 127 || c == 8) { // Backspace
                if (pos > 0) {
                    pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
            }
            else if (c >= 32 && c < 127) { // Печатные символы
                if (pos < MAX_QUERY_LENGTH - 1) {
                    command[pos++] = c;
                    printf("%c", c);
                    fflush(stdout);
                }
            }
        }
    }
}

int main() {
    printf("ODQ SQL Console with AVL Indexing\n");
    printf("Type 'HELP' for available commands\n\n");
    
    while (1) {
        char* command = read_command_with_history();
        if (strlen(command) > 0) {
            process_command(command);
        }
    }
    
    return 0;
}
