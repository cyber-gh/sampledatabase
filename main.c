#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lib.h"
#include "logger.cpp"

uint32_t* internal_node_key(void* node,uint32_t key_num);

uint32_t* internal_node_child(void* node, uint32_t child_num);

uint32_t* internal_node_right_child(void* node);

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);

void indent(uint32_t level){
    for(uint32_t i = 0; i < level; i++){
        printf(" ");
    }
}

void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level){
    void* node = get_page(pager, page_num);
    uint32_t num_keys, child;

    switch(get_node_type(node)){
        case NODE_LEAF:
            num_keys = *leaf_node_num_cells(node);
            indent(indentation_level);
            printf("-  leaf (size %d)\n", num_keys);
            for(uint32_t i = 0; i < num_keys; i++){
                indent(indentation_level + 1);
                printf("- %d\n", *leaf_node_key(node, i));
            }
            break;
        case NODE_INTERNAL:
            num_keys = *internal_node_num_keys(node);
            indent(indentation_level);
            printf("- internal (size %d)\n", num_keys);
            for(uint32_t i = 0; i < num_keys; i++){
                child = *internal_node_child(node, i);
                print_tree(pager, child, indentation_level + 1);

                indent(indentation_level);
                printf("- key %d\n", *internal_node_key(node, i));
            }
            child = *internal_node_right_child(node);
            print_tree(pager, child, indentation_level + 1);
            break;
    }
}

void initialize_internal_node(void* node){
    set_node_type(node, NODE_INTERNAL);
    set_node_root(node, false);
    *internal_node_num_keys(node) = 0;
}

bool is_node_root(void* node){
    uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
    return (bool)value;
}

void set_node_root(void* node, bool is_root){
    uint8_t value = is_root;
    *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
}

// functionality for the internal nodes
uint32_t* internal_node_num_keys(void* node){
    return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t* internal_node_right_child(void* node){
    return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

uint32_t* internal_node_cell(void* node, uint32_t cell_num){
    return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t* internal_node_child(void* node, uint32_t child_num){
    uint32_t num_keys = *internal_node_num_keys(node);
    if (child_num > num_keys){
        printf("Tried to access child num outside limits.\n");
        exit(EXIT_FAILURE);
    } else if (child_num == num_keys){
        return internal_node_right_child(node);
    } else {
        return internal_node_cell(node, child_num);
    }
}

uint32_t* internal_node_key(void* node,uint32_t key_num){
    return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

uint32_t get_node_max_key(void* node){
    switch (get_node_type(node)){
        case NODE_INTERNAL:
            return *internal_node_key(node, *internal_node_num_keys(node) - 1);
        case NODE_LEAF:
            return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);

    }
}

// create a new root
void create_new_root(Table* table, uint32_t right_child_page_num){
    /*
     * handling splitting of the root
     * old root is copied to new page, becomes left child
     * addres of the right child is passed in
     * re initializing root page to contain the new root node
     * new root node points to 2 children*
     * */

    void* root = get_page(table->pager, table->root_page_num);
    void* right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_page_num(table->pager);
    void* left_child = get_page(table->pager, left_child_page_num);

    // copy the old root to the left child
    memcpy(left_child, root, PAGE_SIZE);
    set_node_root(left_child, false);

    // initiliaze the root page as a new internal node with 2 children
    initialize_internal_node(root);
    set_node_root(root, true);
    *internal_node_num_keys(root) = 1;
    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(left_child);
    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;

}
/*
 * until recycling is implemented this one will be used
 * allocates a page at the end of database
 */

 uint32_t get_unused_page_num(Pager* pager){
     return pager->num_pages;
 }

void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value){
    /* creates a new node and move half the cells over,
     * inserts a new value in one of the two nodes
     * updates parent or creates a new paren
    */

    void* old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t new_page_num = get_unused_page_num(cursor->table->pager );
    void* new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);

    /* all the keys plus the new key are divided
     * evenly between old_node (left) and the new_node(right)
     * starting from the right we move each key to correct position
     * */
    for(int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--){
        void* destination_node;
        if ( i >= LEAF_NODE_LEFT_SPLIT_COUNT){
            destination_node = new_node;
         } else {
            destination_node = old_node;
        }
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void* destination = leaf_node_cell(destination_node, index_within_node);

        if ( i == cursor->cell_num){
            serialize_row(value, destination);

        } else if ( i > cursor->cell_num){
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        } else {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }
    // update cell count on both leaf nodes
    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;

    // updating each node parent
    if (is_node_root(old_node)) {
        return create_new_root(cursor->table, new_page_num);
    } else {
        printf("To be implemented: updating a parent after splitting #lazy");
        exit(EXIT_FAILURE);
    }


}

// checks the type of a node
NodeType get_node_type(void* node){
    uint8_t value = *((uint8_t*)(node + NOTE_TYPE_OFFSET));
}
void set_node_type(void* node, NodeType type){
    uint8_t value = type;
    *((uint8_t*)(node + NOTE_TYPE_OFFSET)) = value;
}

// binary search in a node to find the key
Cursor *leaf_node_find(Table * table, uint32_t page_num, uint32_t key) {
    void* node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    // the actual binary search
    uint32_t min = 0;
    uint32_t max = num_cells;

    while(min != max){
        uint32_t mid = (min + max) / 2;
        uint32_t key_mid = *leaf_node_key(node, mid);
        if (key == key_mid){
            cursor->cell_num = mid;
            return cursor;
        }
        if (key < key_mid){
            max = mid;
        } else {
            min = mid + 1;
        }
    }

    cursor->cell_num = min;
    return cursor;

}


// retunrs the position of the given key or the postion where it should be isnerted
Cursor* table_find(Table* table, uint32_t key){
    uint32_t root_page_num = table->root_page_num;
    void* root_node = get_page(table->pager, root_page_num);

    if (get_node_type(root_node) == NODE_LEAF){
        return leaf_node_find(table, root_page_num, key);
    } else {
        return internal_node_find(table, root_page_num, key);
    }
}

Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key) {
    void* node = get_page(table->pager, page_num);
    uint32_t num_keys = *internal_node_num_keys(node);

    // binary search to find index of child to search
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while(min_index != max_index){
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);
        if (key_to_right >= key){
            max_index = index;
        } else {
            min_index = index;
        }
    }
    uint32_t child_num = *internal_node_child(node, min_index);
    void* child = get_page(table->pager, child_num);
    switch(get_node_type(child)){
        case NODE_LEAF:
            return leaf_node_find(table, child_num, key);
        case NODE_INTERNAL:
            return internal_node_find(table, child_num, key);

    }
}

void print_constants(){
    printf("ROW_SIZE: %ld\n", ROW_SIZE);
    printf("COMMON_NODE_HEADER_SIZE: %ld\n", COMMON_NODE_HEADER_SIZE);
    printf("LEAF_NODE_HEADER_SIZE: %ld\n", LEAF_NODE_HEADER_SIZE);
    printf("LEAF_NODE_CELL_SIZE: %ld\n", LEAF_NODE_CELL_SIZE);
    printf("LEAF_NODE_SPACE_FOR_CELLS: %ld\n", LEAF_NODE_SPACE_FOR_CELLS);
    printf("LEAF_NODE_MAX_CELLS: %ld\n", LEAF_NODE_MAX_CELLS);

}

void print_leaf_node(void* node){
    uint32_t num_cells = *leaf_node_num_cells(node);
    printf("leaf (size %d)\n", num_cells);
    for(uint32_t i = 0; i < num_cells; i++){
        uint32_t key = *leaf_node_key(node, i);
        printf("   - %d : %d\n", i, key);
    }
}

void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value){
    void* node = get_page(cursor->table->pager, cursor->page_num);

    uint32_t num_cells = *leaf_node_num_cells(node);
    if (num_cells >= LEAF_NODE_MAX_CELLS){
        // node full
        leaf_node_split_and_insert(cursor, key, value);
        return;
    }

    if (cursor->cell_num < num_cells){
        // making room for a new cell
        for(uint32_t i = num_cells; i > cursor->cell_num; i--){
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));

}

uint32_t* leaf_node_num_cells(void* node){
    return (char* )node + LEAF_NODE_NUM_CELLS_OFFSET;
}

void* leaf_node_cell(void* node, uint32_t cell_num){
    return (char* )node + LEAF_NODE_HEADER_SIZE + cell_num;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num){
    return leaf_node_cell(node, cell_num);
}

void* leaf_node_value(void* node, uint32_t cell_num){
    return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void* node){
    set_node_type(node, NODE_LEAF);
    set_node_root(node, false);
    *leaf_node_num_cells(node) = 0;
}

void cursor_advance(Cursor* cursor){
    uint32_t page_num = cursor->page_num;
    void* node = get_page(cursor->table->pager, page_num);

    cursor->cell_num += 1;
    if (cursor->cell_num >= (*leaf_node_num_cells(node))){
        cursor->end_of_table = true;
    }
}

void pager_flush(Pager* pager, uint32_t page_num){
    fprintf(LOGHANDLER, "FLusing pager\n");
    if (pager->pages[page_num] == NULL){
        printf("Tried to flush NULL page\n");
        exit(EXIT_FAILURE);
    }
    fprintf(LOGHANDLER, "pager file handler=%d\n",pager->file_descriptor);
    off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if (offset == -1){
        printf("Error seeking: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);

    if (bytes_written == -1){
        printf("Error writing: %d\n", errno);
        exit(EXIT_FAILURE);
    }
}

void db_close(Table* table){
    Pager* pager = table->pager;

    for(uint32_t i = 0; i < pager->num_pages; i++){
        if (pager->pages[i] == NULL){
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }


    fprintf(LOGHANDLER, "Flushed all pages\n");
    int result = close(pager->file_descriptor);
    if (result == -1){
        printf("Error closing db file.\n");
        exit(EXIT_FAILURE);
    }

    for(uint32_t i = 0; i < TABLE_MAX_PAGES; i++){
        void* page = pager->pages[i];
        if (page){
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);

    fprintf(LOGHANDLER,"Closing database\n");
}

void* get_page(Pager* pager, uint32_t page_num){
    if (page_num > TABLE_MAX_PAGES){
        printf("Tried to fetch page number out of bonds. %d", TABLE_MAX_PAGES);
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == NULL){
        void* page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (pager->file_length % PAGE_SIZE){
            num_pages += 1;
        }

        if (page_num <= num_pages){
            lseek(pager->file_descriptor, page_num*PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1){
                printf("Error reading file: %d\n", errno);
                exit(EXIT_FAILURE);
            }
        }

        pager->pages[page_num] = page;

        if (page_num >= pager->num_pages){
            pager->num_pages = page_num + 1;
        }
    }

    return pager->pages[page_num];
}

Pager* pager_open(const char* filename){
    fprintf(LOGHANDLER, "Opening the file\n");
    // open file read/write mode - create file if it doesn't exist
    // user write permissions - user read permissions
    int open_result = open(filename, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);

    if (open_result == -1){
        printf("Unable to open file\n");
        exit(EXIT_FAILURE);
    }

    off_t file_length = lseek(open_result, 0, SEEK_END);

    Pager* pager = malloc(sizeof(Pager));
    pager->file_descriptor = open_result;
    pager->file_length = file_length;
    pager->num_pages = (file_length / PAGE_SIZE);

    if (file_length % PAGE_SIZE != 0){
        printf("Corrupt database file, there is not a whole number of pages");
        exit(EXIT_FAILURE);
    }

    fprintf(LOGHANDLER, "file length=%d\n", pager->file_length);
    fprintf(LOGHANDLER, "pager file descrpitor=%d\n", open_result);
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++){
        pager->pages[i] = NULL;
    }
    fprintf(LOGHANDLER, "File opened\n");
    return pager;

}

Table* db_open(const char* filename) {
    fprintf(LOGHANDLER, "Opening database\n");

    Pager* pager = pager_open(filename);

    Table* table;
    table = malloc(sizeof(Table));
    table->pager = pager;

    if(pager->num_pages == 0){
        // new database file. Initilizing page 0 as leaf node (empty node)
        void* root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_node_root(root_node, true);

    }

    fprintf(LOGHANDLER, "size of pager=%d\n", pager->file_length);
    fprintf(LOGHANDLER, "Allocating memory for table\n");
    fprintf(LOGHANDLER, "Database opened\n");

    return table;
}

// pointer to the place in memory of a row
void* cursor_value(Cursor* cursor){
    uint32_t page_num = cursor->page_num;
    void* page = get_page(cursor->table->pager, page_num);

    return leaf_node_value(page, cursor->cell_num);

}

void deserialize_row(void* source, Row* destination){
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

//initializing a new buffer
InputBuffer* new_input_buffer() {
    InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table){
    if (strcmp(input_buffer->buffer, ".exit") == 0){
        db_close(table);
        exit(EXIT_SUCCESS);
    } else if (strcmp(input_buffer->buffer, ".constants") == 0){
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    } else if (strcmp(input_buffer->buffer, ".btree") == 0){
        printf("Tree:\n");
        print_tree(table->pager, 0 , 0);
        return META_COMMAND_SUCCESS;
    }
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){

    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }
    if (strcmp(input_buffer->buffer, "select") == 0){
       statement->type = STATEMENT_SELECT;
       return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement* statement, Table* table){

    void* node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = (*leaf_node_num_cells(node));


    Row* row_to_insert = &(statement->row_to_insert);
    uint32_t key_to_insert = row_to_insert->id;
    Cursor* cursor = table_find(table, key_to_insert);

    if (cursor->cell_num < num_cells){
        uint32_t key_at_index = *leaf_node_key(node,cursor->cell_num);
        if (key_at_index == key_to_insert){
            return EXECUTE_DUPLICATE_KEY;
        }
    }
    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

    return EXECUTE_SUCCESS;

}

ExecuteResult execute_select(Statement* statement, Table* table){
    Cursor* cursor = table_start(table);

    Row row;
    while(!(cursor->end_of_table)){
        deserialize_row(cursor_value(cursor), &row);
        print_row(&row);
        cursor_advance(cursor);
    }

    free(cursor);

    return EXECUTE_SUCCESS;
}


ExecuteResult execute_statement(Statement* statement, Table* table){


    switch (statement->type){
        case (STATEMENT_INSERT):
            return execute_insert(statement, table);
        case (STATEMENT_SELECT):
            return execute_select(statement, table);
    }
}

//prints row
void print_row(Row* row){
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void serialize_row(Row *source, void *destination) {

    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);


}

Cursor* table_start(Table* table){
    Cursor* cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = table->root_page_num;
    cursor->cell_num = 0;

    void* root_node = get_page(table->pager, table->root_page_num);
    uint32_t num_cells = *leaf_node_num_cells(root_node);
    cursor->end_of_table = (num_cells == 0);

    return cursor;

}

void print_prompt() {
    printf("db > ");
}

void read_input(InputBuffer* input_buffer) {
    ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

    if (bytes_read <= 0){
        printf("Error reading input\n");
        exit(EXIT_FAILURE);
    }

    //Ignore the newline in the end
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement){
    statement->type = STATEMENT_INSERT;

    char* keyword = strtok(input_buffer->buffer, " ");
    char* id_string = strtok(NULL, " ");
    char* username = strtok(NULL, " ");
    char* email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL){
        return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string); // to do check if id it's not a number
    if (id < 0){
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }

    if (strlen(email) > COLUMN_EMAIL_SIZE){
        return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);

    return PREPARE_SUCCESS;

}

int main(int argc, char* argv[]) {
    DeleteLog();
    LOGHANDLER = fopen(LOGFILE,"w");
    fprintf(LOGHANDLER, "Started debugging\n");
    if (argc < 2) {
        printf("Must supply a database name.\n");
        exit(EXIT_FAILURE);
    }
    char* filename = argv[1];
    Table* table = db_open(filename);
    InputBuffer* input_buffer = new_input_buffer();
    fprintf(LOGHANDLER, "Entering the main loop\n");
    while(true){
        print_prompt();
        read_input(input_buffer);
        if (input_buffer->buffer[0] == '.'){
            switch(do_meta_command(input_buffer, table)){
                case (META_COMMAND_SUCCESS):
                    continue;
                case (META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                    continue;
            }
        }

        Statement statement;
        switch(prepare_statement(input_buffer, &statement)){
            case (PREPARE_SUCCESS):
                break;
            case (PREPARE_SYNTAX_ERROR):
                printf("Syntax error. Could not parse statement \n");
                break;
            case (PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                continue;
            case PREPARE_STRING_TOO_LONG:
                printf("String is too long.\n");
                continue;
            case PREPARE_NEGATIVE_ID:
                printf("ID must be positive.\n");
                continue;
        }

        switch (execute_statement(&statement, table)) {
            case (EXECUTE_SUCCESS):
                printf("Executed.\n");
                break;
            case (EXECUTE_TABLE_FULL):
                printf("Error: Table full.\n");
                break;
            case EXECUTE_DUPLICATE_KEY:
                printf("Error duplicate key.\n");
                break;
        }
    }

    return 0;
}
