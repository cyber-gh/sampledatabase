//
// Created by cyber-gh on 8/20/18.
//
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "logger.h"
#ifndef SQLITE_LIB_H
#define SQLITE_LIB_H

#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

#define ID_SIZE  size_of_attribute(Row, id)
#define USERNAME_SIZE  size_of_attribute(Row, username)
#define EMAIL_SIZE  size_of_attribute(Row, email)
#define ID_OFFSET  0
#define USERNAME_OFFSET  (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET  (USERNAME_OFFSET + USERNAME_SIZE)
#define ROW_SIZE (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)

// static table data structure
#define PAGE_SIZE  4096
#define TABLE_MAX_PAGES  100
#define ROWS_PER_PAGE  (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS  (ROWS_PER_PAGE * ROW_SIZE)

#define COLUMN_USERNAME_SIZE  32
#define COLUMN_EMAIL_SIZE  255

// Node in B+ tree constants

// intermedie header node constants
#define NOTE_TYPE_SIZE sizeof(uint8_t)
#define NOTE_TYPE_OFFSET 0
#define IS_ROOT_SIZE sizeof(uint8_t)
#define IS_ROOT_OFFSET sizeof(uint8_t)
#define PARENT_POINTER_SIZE sizeof(uint32_t)
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NOTE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

// leaf header node constants
#define LEAF_NODE_NUM_CELLS_SIZE (sizeof(uint32_t))
#define LEAF_NODE_NUM_CELLS_OFFSET (COMMON_NODE_HEADER_SIZE)
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE)

// leaf body layout
#define LEAF_NODE_KEY_SIZE (sizeof(uint32_t))
#define LEAF_NODE_KEY_OFFSET 0
#define LEAF_NODE_VALUE_SIZE ROW_SIZE
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE )
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

// leaf splitting constants
#define LEAF_NODE_RIGHT_SPLIT_COUNT (LEAF_NODE_MAX_CELLS + 1) / 2
#define LEAF_NODE_LEFT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT)

// internal node header layout
#define INTERNAL_NODE_NUM_KEYS_SIZE (sizeof(uint32_t))
#define INTERNAL_NODE_NUM_KEYS_OFFSET (COMMON_NODE_HEADER_SIZE)
#define INTERNAL_NODE_RIGHT_CHILD_SIZE (sizeof(uint32_t))
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

// internal node mode layout
#define INTERNAL_NODE_KEY_SIZE (sizeof(uint32_t))
#define INTERNAL_NODE_CHILD_SIZE (sizeof(uint32_t))
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)


FILE* LOGHANDLER;

struct Row_t{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
};
typedef struct Row_t Row;

enum ExecuteResult_t {
    EXECUTE_TABLE_FULL,
    EXECUTE_SUCCESS,
    EXECUTE_DUPLICATE_KEY
};
typedef enum ExecuteResult_t ExecuteResult;

enum StatementType_t {
    STATEMENT_INSERT,
    STATEMENT_SELECT
};
typedef enum StatementType_t StatementType;

struct Statement_t{
    StatementType type;
    Row row_to_insert; // used as insert statement
};
typedef struct Statement_t Statement;

enum MetaCommandResult_t {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
};
typedef enum MetaCommandResult_t MetaCommandResult;

enum PrepareResult_t {
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
    PREPARE_NEGATIVE_ID
};
typedef enum PrepareResult_t PrepareResult;

struct InputBuffer_t{
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
};
typedef struct InputBuffer_t InputBuffer;

struct Pager_t{
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void* pages[TABLE_MAX_PAGES];
};
typedef  struct Pager_t Pager;

struct Table_t{
    Pager* pager;
    uint32_t root_page_num;
};
typedef struct Table_t Table;

struct Cursor_t{
    Table* table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table; // indicates one position past the end
};
typedef struct Cursor_t Cursor;

enum NodeType_t{
    NODE_INTERNAL,
    NODE_LEAF
};


typedef enum NodeType_t NodeType;

void print_row(Row* row);
void serialize_row(Row* source, void* destination);
PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement);
Pager* pager_open(const char* filename);
void* get_page(Pager* pager, uint32_t page_num);
void db_close(Table* table);
void pager_flush(Pager*, uint32_t);
Cursor* table_start(Table*);
Cursor* table_end(Table*);
void cursor_advance(Cursor*);

// funcitonality for accessing leaf ndoe fields
uint32_t* leaf_node_num_cells(void*);
void* leaf_node_cell(void*, uint32_t);
uint32_t* leaf_node_key(void*, uint32_t);
void* leaf_node_value(void*, uint32_t);
void initialize_leaf_node(void*);
void leaf_node_insert(Cursor*, uint32_t, Row*);

// to help with debuggin
void print_constants();
void print_leaf_node();

Cursor* table_find(Table*, uint32_t);
Cursor* leaf_node_find(Table*, uint32_t, uint32_t);
uint32_t get_unused_page_num(Pager* pager);
NodeType get_node_type(void* node);
void set_node_type(void* node, NodeType type);
void set_node_root(void* node, bool is_root);
uint32_t* internal_node_num_keys(void* node);

#endif //SQLITE_LIB_H
