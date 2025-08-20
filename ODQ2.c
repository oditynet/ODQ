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

typedef struct {
    char field_name[MAX_FIELD_NAME];
    char operator[10];
    char value[100];
    bool is_and;
} WhereCondition;

typedef struct {
    char table1[MAX_TABLE_NAME];
    char table2[MAX_TABLE_NAME];
    char field1[MAX_FIELD_NAME];
    char field2[MAX_FIELD_NAME];
    char join_type[20];
} JoinInfo;

// Глобальные переменные
Table current_table;
bool table_loaded = false;
char command_history[HISTORY_SIZE][MAX_QUERY_LENGTH];
int history_count = 0;
int history_current = -1;
struct termios orig_termios;

// Прототипы функций
void disable_raw_mode();
void enable_raw_mode();
void add_to_history(const char* command);
const char* get_history_command(int direction);
int height(AVLNode* node);
int max(int a, int b);
AVLNode* newAVLNode(const char* key, long position);
AVLNode* rightRotate(AVLNode* y);
AVLNode* leftRotate(AVLNode* x);
int getBalance(AVLNode* node);
AVLNode* insertAVL(AVLNode* node, const char* key, long position);
AVLNode* searchAVL(AVLNode* root, const char* key);
void searchTextInAVL(AVLNode* root, const char* search_text, long** results, int* count);
void create_table(const char* table_name, const char* field_definitions);
bool load_table(const char* table_name);
void insert_into_table(const char* values);
void select_all();
void select_field(const char* field_name);
bool compare_values(const char* field_value, const char* operator, const char* compare_value, FieldType field_type);
WhereCondition* parse_where_conditions(const char* where_clause, int* condition_count);
bool check_single_condition(char* record, WhereCondition* condition);
bool check_complex_conditions(char* record, WhereCondition* conditions, int condition_count);
void select_where(const char* field_name, const char* operator, const char* value);
void find_text(const char* search_text);
void load_macro(const char* filename);
void select_columns(const char* columns, const char* where_clause);
void select_count(const char* where_clause);
bool load_table_from_file(const char* filename, Table* table);
void inner_join(Table* table1, Table* table2, const char* field1, const char* field2);
void left_join(Table* table1, Table* table2, const char* field1, const char* field2);
void right_join(Table* table1, Table* table2, const char* field1, const char* field2);
void full_join(Table* table1, Table* table2, const char* field1, const char* field2);
void perform_join(JoinInfo* join_info);
void process_command(const char* command);
char* read_command_with_history();

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
    
    if (direction == 1) {
        if (history_current > 0) history_current--;
    } else {
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

bool load_table_from_file(const char* filename, Table* table) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return false;
    }
    
    if (fread(table, sizeof(Table), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    fclose(file);
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
    
    printf("1 row inserted\n");
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

WhereCondition* parse_where_conditions(const char* where_clause, int* condition_count) {
    WhereCondition* conditions = malloc(10 * sizeof(WhereCondition));
    *condition_count = 0;
    
    char clause_copy[MAX_QUERY_LENGTH];
    strcpy(clause_copy, where_clause);
    
    char* token = strtok(clause_copy, " ");
    int state = 0;
    
    WhereCondition current_condition;
    memset(&current_condition, 0, sizeof(WhereCondition));
    
    while (token != NULL && *condition_count < 10) {
        if (state == 0) {
            strcpy(current_condition.field_name, token);
            state = 1;
        }
        else if (state == 1) {
            strcpy(current_condition.operator, token);
            state = 2;
        }
        else if (state == 2) {
            if (token[0] == '\'') {
                char value[100] = {0};
                strcpy(value, token + 1);
                while (token != NULL && value[strlen(value)-1] != '\'') {
                    token = strtok(NULL, " ");
                    if (token) {
                        strcat(value, " ");
                        strcat(value, token);
                    }
                }
                if (value[strlen(value)-1] == '\'') {
                    value[strlen(value)-1] = '\0';
                }
                strcpy(current_condition.value, value);
            } else {
                strcpy(current_condition.value, token);
            }
            
            conditions[(*condition_count)++] = current_condition;
            memset(&current_condition, 0, sizeof(WhereCondition));
            state = 3;
        }
        else if (state == 3) {
            if (strcasecmp(token, "AND") == 0) {
                current_condition.is_and = true;
            } else if (strcasecmp(token, "OR") == 0) {
                current_condition.is_and = false;
            }
            state = 0;
        }
        
        token = strtok(NULL, " ");
    }
    
    return conditions;
}

bool check_single_condition(char* record, WhereCondition* condition) {
    int field_index = -1;
    for (int i = 0; i < current_table.field_count; i++) {
        if (strcmp(current_table.fields[i].name, condition->field_name) == 0) {
            field_index = i;
            break;
        }
    }
    
    if (field_index == -1) return false;
    
    int offset = 0;
    for (int i = 0; i < field_index; i++) {
        offset += current_table.fields[i].size;
    }
    
    Field field = current_table.fields[field_index];
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
    
    return compare_values(field_value, condition->operator, condition->value, field.type);
}

bool check_complex_conditions(char* record, WhereCondition* conditions, int condition_count) {
    if (condition_count == 0) return true;
    
    bool result = check_single_condition(record, &conditions[0]);
    
    for (int i = 1; i < condition_count; i++) {
        bool current_result = check_single_condition(record, &conditions[i]);
        
        if (conditions[i].is_and) {
            result = result && current_result;
        } else {
            result = result || current_result;
        }
    }
    
    return result;
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

void select_columns(const char* columns, const char* where_clause) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    char cols_copy[MAX_QUERY_LENGTH];
    strcpy(cols_copy, columns);
    
    int selected_columns[MAX_FIELDS];
    int selected_count = 0;
    bool select_all = false;
    
    if (strcmp(columns, "*") == 0) {
        select_all = true;
        selected_count = current_table.field_count;
        for (int i = 0; i < selected_count; i++) {
            selected_columns[i] = i;
        }
    } else {
        char* token = strtok(cols_copy, ",");
        while (token != NULL && selected_count < MAX_FIELDS) {
            while (*token == ' ') token++;
            char* end = token + strlen(token) - 1;
            while (end > token && *end == ' ') *end-- = '\0';
            
            for (int i = 0; i < current_table.field_count; i++) {
                if (strcmp(current_table.fields[i].name, token) == 0) {
                    selected_columns[selected_count++] = i;
                    break;
                }
            }
            
            token = strtok(NULL, ",");
        }
    }
    
    int condition_count = 0;
    WhereCondition* conditions = NULL;
    
    if (where_clause != NULL && strlen(where_clause) > 0) {
        conditions = parse_where_conditions(where_clause, &condition_count);
    }
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        if (conditions == NULL || check_complex_conditions(record, conditions, condition_count)) {
            for (int i = 0; i < selected_count; i++) {
                int field_idx = selected_columns[i];
                Field field = current_table.fields[field_idx];
                int offset = 0;
                
                for (int j = 0; j < field_idx; j++) {
                    offset += current_table.fields[j].size;
                }
                
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
                
                if (i < selected_count - 1) printf(" | ");
            }
            printf("\n");
            count++;
        }
    }
    
    printf("%d rows returned\n", count);
    
    if (conditions != NULL) {
        free(conditions);
    }
}

void select_count(const char* where_clause) {
    if (!table_loaded) {
        printf("No table selected\n");
        return;
    }
    
    int condition_count = 0;
    WhereCondition* conditions = NULL;
    
    if (where_clause != NULL && strlen(where_clause) > 0) {
        conditions = parse_where_conditions(where_clause, &condition_count);
    }
    
    fseek(current_table.data_file, sizeof(Table), SEEK_SET);
    char record[MAX_RECORD_SIZE];
    int count = 0;
    
    while (fread(record, current_table.record_size, 1, current_table.data_file)) {
        if (conditions == NULL || check_complex_conditions(record, conditions, condition_count)) {
            count++;
        }
    }
    
    printf("COUNT: %d\n", count);
    
    if (conditions != NULL) {
        free(conditions);
    }
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

// JOIN functions
void inner_join(Table* table1, Table* table2, const char* field1, const char* field2) {
    printf("Performing INNER JOIN on %s.%s = %s.%s\n", 
           table1->name, field1, table2->name, field2);
    
    // Find field indices
    int field_idx1 = -1, field_idx2 = -1;
    for (int i = 0; i < table1->field_count; i++) {
        if (strcmp(table1->fields[i].name, field1) == 0) {
            field_idx1 = i;
            break;
        }
    }
    for (int i = 0; i < table2->field_count; i++) {
        if (strcmp(table2->fields[i].name, field2) == 0) {
            field_idx2 = i;
            break;
        }
    }
    
    if (field_idx1 == -1 || field_idx2 == -1) {
        printf("Join fields not found\n");
        return;
    }
    
    // Simple nested loop join implementation
    FILE* file1 = fopen(table1->filename, "rb");
    FILE* file2 = fopen(table2->filename, "rb");
    
    if (!file1 || !file2) {
        printf("Error opening table files\n");
        if (file1) fclose(file1);
        if (file2) fclose(file2);
        return;
    }
    
    fseek(file1, sizeof(Table), SEEK_SET);
    fseek(file2, sizeof(Table), SEEK_SET);
    
    char record1[MAX_RECORD_SIZE];
    char record2[MAX_RECORD_SIZE];
    int join_count = 0;
    
    while (fread(record1, table1->record_size, 1, file1)) {
        // Get join value from table1
        int offset1 = 0;
        for (int i = 0; i < field_idx1; i++) offset1 += table1->fields[i].size;
        
        char join_value1[256] = {0};
        if (table1->fields[field_idx1].type == FIELD_INT) {
            int val;
            memcpy(&val, record1 + offset1, sizeof(int));
            snprintf(join_value1, sizeof(join_value1), "%d", val);
        } else {
            strncpy(join_value1, record1 + offset1, table1->fields[field_idx1].size);
        }
        
        // Reset file2 pointer for each record in table1
        fseek(file2, sizeof(Table), SEEK_SET);
        
        while (fread(record2, table2->record_size, 1, file2)) {
            // Get join value from table2
            int offset2 = 0;
            for (int i = 0; i < field_idx2; i++) offset2 += table2->fields[i].size;
            
            char join_value2[256] = {0};
            if (table2->fields[field_idx2].type == FIELD_INT) {
                int val;
                memcpy(&val, record2 + offset2, sizeof(int));
                snprintf(join_value2, sizeof(join_value2), "%d", val);
            } else {
                strncpy(join_value2, record2 + offset2, table2->fields[field_idx2].size);
            }
            
            if (strcmp(join_value1, join_value2) == 0) {
                // Print joined record
                printf("Joined record %d:\n", ++join_count);
                
                // Table1 fields
                int offset = 0;
                for (int i = 0; i < table1->field_count; i++) {
                    Field field = table1->fields[i];
                    printf("%s.%s: ", table1->name, field.name);
                    
                    switch (field.type) {
                        case FIELD_INT: {
                            int val;
                            memcpy(&val, record1 + offset, sizeof(int));
                            printf("%d", val);
                            break;
                        }
                        case FIELD_TEXT: {
                            char text[field.size + 1];
                            strncpy(text, record1 + offset, field.size);
                            text[field.size] = '\0';
                            printf("'%s'", text);
                            break;
                        }
                        case FIELD_BOOL: {
                            bool val;
                            memcpy(&val, record1 + offset, sizeof(bool));
                            printf("%s", val ? "true" : "false");
                            break;
                        }
                    }
                    offset += field.size;
                    printf(" | ");
                }
                
                // Table2 fields
                offset = 0;
                for (int i = 0; i < table2->field_count; i++) {
                    Field field = table2->fields[i];
                    printf("%s.%s: ", table2->name, field.name);
                    
                    switch (field.type) {
                        case FIELD_INT: {
                            int val;
                            memcpy(&val, record2 + offset, sizeof(int));
                            printf("%d", val);
                            break;
                        }
                        case FIELD_TEXT: {
                            char text[field.size + 1];
                            strncpy(text, record2 + offset, field.size);
                            text[field.size] = '\0';
                            printf("'%s'", text);
                            break;
                        }
                        case FIELD_BOOL: {
                            bool val;
                            memcpy(&val, record2 + offset, sizeof(bool));
                            printf("%s", val ? "true" : "false");
                            break;
                        }
                    }
                    offset += field.size;
                    if (i < table2->field_count - 1) printf(" | ");
                }
                printf("\n---\n");
            }
        }
    }
    
    fclose(file1);
    fclose(file2);
    printf("INNER JOIN completed. %d records joined.\n", join_count);
}

void left_join(Table* table1, Table* table2, const char* field1, const char* field2) {
    printf("LEFT JOIN not fully implemented yet\n");
    inner_join(table1, table2, field1, field2);
}

void right_join(Table* table1, Table* table2, const char* field1, const char* field2) {
    printf("RIGHT JOIN not fully implemented yet\n");
    inner_join(table1, table2, field1, field2);
}

void full_join(Table* table1, Table* table2, const char* field1, const char* field2) {
    printf("FULL JOIN not fully implemented yet\n");
    inner_join(table1, table2, field1, field2);
}

void perform_join(JoinInfo* join_info) {
    Table table1, table2;
    char filename1[100], filename2[100];
    
    snprintf(filename1, sizeof(filename1), "%s_%s.bin", TABLE_PREFIX, join_info->table1);
    snprintf(filename2, sizeof(filename2), "%s_%s.bin", TABLE_PREFIX, join_info->table2);
    
    if (!load_table_from_file(filename1, &table1)) {
        printf("Table %s not found\n", join_info->table1);
        return;
    }
    
    if (!load_table_from_file(filename2, &table2)) {
        printf("Table %s not found\n", join_info->table2);
        return;
    }
    
    if (strcasecmp(join_info->join_type, "INNER") == 0) {
        inner_join(&table1, &table2, join_info->field1, join_info->field2);
    }
    else if (strcasecmp(join_info->join_type, "LEFT") == 0) {
        left_join(&table1, &table2, join_info->field1, join_info->field2);
    }
    else if (strcasecmp(join_info->join_type, "RIGHT") == 0) {
        right_join(&table1, &table2, join_info->field1, join_info->field2);
    }
    else if (strcasecmp(join_info->join_type, "FULL") == 0) {
        full_join(&table1, &table2, join_info->field1, join_info->field2);
    }
    else {
        printf("Unknown JOIN type: %s\n", join_info->join_type);
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
            printf("\n[Line %d] %s\n", line_num, line);
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
        if (strstr(rest, "COUNT(*)") != NULL) {
            char where_clause[MAX_QUERY_LENGTH] = {0};
            char* where_pos = strstr(rest, "WHERE");
            if (where_pos) {
                strcpy(where_clause, where_pos + 6);
            }
            select_count(where_clause);
        }
        else if (strstr(rest, "JOIN") != NULL) {
            // Basic JOIN parsing
            JoinInfo join_info;
            memset(&join_info, 0, sizeof(JoinInfo));
            
            char* join_pos = strstr(rest, "JOIN");
            if (join_pos) {
                char* on_pos = strstr(join_pos, "ON");
                if (on_pos) {
                    *on_pos = '\0';
                    char* table2_start = join_pos + 5;
                    while (*table2_start == ' ') table2_start++;
                    
                    char* table2_end = table2_start;
                    while (*table2_end && *table2_end != ' ') table2_end++;
                    *table2_end = '\0';
                    
                    strcpy(join_info.table2, table2_start);
                    
                    char* on_clause = on_pos + 3;
                    char* dot_pos = strchr(on_clause, '.');
                    if (dot_pos) {
                        *dot_pos = '\0';
                        strcpy(join_info.field1, on_clause);
                        
                        char* equal_pos = strchr(dot_pos + 1, '=');
                        if (equal_pos) {
                            *equal_pos = '\0';
                            strcpy(join_info.field2, dot_pos + 1);
                            
                            char* table2_field = equal_pos + 1;
                            while (*table2_field == ' ') table2_field++;
                            
                            char* dot2_pos = strchr(table2_field, '.');
                            if (dot2_pos) {
                                *dot2_pos = '\0';
                                strcpy(join_info.table1, table2_field);
                                
                                char* field2_name = dot2_pos + 1;
                                strcpy(join_info.field2, field2_name);
                                
                                // Determine JOIN type
                                if (strstr(rest, "INNER")) strcpy(join_info.join_type, "INNER");
                                else if (strstr(rest, "LEFT")) strcpy(join_info.join_type, "LEFT");
                                else if (strstr(rest, "RIGHT")) strcpy(join_info.join_type, "RIGHT");
                                else if (strstr(rest, "FULL")) strcpy(join_info.join_type, "FULL");
                                else strcpy(join_info.join_type, "INNER");
                                
                                perform_join(&join_info);
                            }
                        }
                    }
                }
            }
        }
        else {
            char columns[200] = {0};
            char from_part[200] = {0};
            char where_clause[MAX_QUERY_LENGTH] = {0};
            
            char* from_pos = strstr(rest, "FROM");
            if (from_pos) {
                strncpy(columns, rest, from_pos - rest);
                columns[from_pos - rest] = '\0';
                
                char* where_pos = strstr(from_pos, "WHERE");
                if (where_pos) {
                    strncpy(from_part, from_pos + 5, where_pos - from_pos - 5);
                    from_part[where_pos - from_pos - 5] = '\0';
                    strcpy(where_clause, where_pos + 6);
                } else {
                    strcpy(from_part, from_pos + 5);
                }
                
                char* trimmed_columns = columns;
                while (*trimmed_columns == ' ') trimmed_columns++;
                char* end = trimmed_columns + strlen(trimmed_columns) - 1;
                while (end > trimmed_columns && *end == ' ') *end-- = '\0';
                
                select_columns(trimmed_columns, where_clause);
            } else {
                printf("Syntax error: missing FROM clause\n");
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
        printf("  SELECT col1, col2 FROM tablename\n");
        printf("  SELECT * FROM tablename WHERE condition\n");
        printf("  SELECT COUNT(*) FROM tablename [WHERE condition]\n");
        printf("  SELECT * FROM table1 JOIN table2 ON table1.col = table2.col\n");
        printf("  FIND TEXT 'searchtext'\n");
        printf("  LOAD filename\n");
        printf("  EXIT\n");
    }
    else {
        printf("Unknown command: %s\n", cmd);
    }
    
    add_to_history(command);
}

void handle_command_line_args(int argc, char* argv[]) {
    if (argc < 2) return;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "LOAD") == 0 && i + 1 < argc) {
            printf("Loading macro from command line: %s\n", argv[i + 1]);
            load_macro(argv[i + 1]);
            i++; // Пропускаем следующий аргумент (имя файла)
        }
        else if (strncmp(argv[i], "LOAD=", 5) == 0) {
            const char* filename = argv[i] + 5;
            printf("Loading macro from command line: %s\n", filename);
            load_macro(filename);
        }
        else {
            // Выполняем команду напрямую
            printf("Executing command: %s\n", argv[i]);
            process_command(argv[i]);
        }
    }
}

// Улучшенная функция чтения команды с историей
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
                        else if (seq[1] == 'C') { // Стрелка вправо
                            // Можно добавить перемещение курсора
                        }
                        else if (seq[1] == 'D') { // Стрелка влево
                            // Можно добавить перемещение курсора
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
            else if (c == 9) { // Tab - автодополнение
                // Можно добавить автодополнение команд
                printf("\nAvailable commands: CREATE, USE, SELECT, INSERT, FIND, LOAD, EXIT\n");
                printf("ODQ> %s", command); // Восстанавливаем prompt
                fflush(stdout);
            }
            else if (c >= 32 && c < 127) { // Печатные символы
                if (pos < MAX_QUERY_LENGTH - 1) {
                    command[pos++] = c;
                    printf("%c", c);
                    fflush(stdout);
                }
            }
            else if (c == 3) { // Ctrl+C
                printf("\n^C\n");
                disable_raw_mode();
                strcpy(command, "");
                return command;
            }
            else if (c == 4) { // Ctrl+D
                printf("\n^D\n");
                disable_raw_mode();
                strcpy(command, "EXIT");
                return command;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    printf("ODQ SQL Console with AVL Indexing\n");
    printf("Type 'HELP' for available commands\n\n");

    // Обрабатываем аргументы командной строки
    if (argc > 1) {
        printf("Processing command line arguments...\n");
        handle_command_line_args(argc, argv);

        // Если были только команды из аргументов - выходим
        if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
            printf("Usage: %s [COMMAND] [LOAD filename] [LOAD=filename]\n", argv[0]);
            printf("Examples:\n");
            printf("  %s \"CREATE TABLE users (id int, name text(50))\"\n", argv[0]);
            printf("  %s LOAD init.macro\n", argv[0]);
            printf("  %s LOAD=init.macro \"USE users\" \"SELECT * FROM users\"\n", argv[0]);
            return 0;
        }

        // Проверяем, нужно ли продолжать интерактивный режим
        bool interactive = true;
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--batch") == 0 || strcmp(argv[i], "-b") == 0) {
                interactive = false;
                break;
            }
        }

        if (!interactive) {
            printf("Batch mode completed.\n");
            return 0;
        }

        printf("\nEntering interactive mode...\n");
    }

    // Основной интерактивный цикл
    while (1) {
        char* command = read_command_with_history();
        if (strlen(command) > 0) {
            process_command(command);
        }
    }

    return 0;
}