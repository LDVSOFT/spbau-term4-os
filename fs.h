#pragma once

#include "threads.h"
#include "memory.h"
#include "list.h"

#define FILE_NAME 256

enum file_type {
	T_REGULAR,
	T_DIRECTORY
};

struct file {
	struct mutex lock;
	enum file_type type;
	// for regular
	uint64_t size;
	int size_level;
	char* data;
	// for directory
	struct list_node entries_head;
};

struct file_desc {
	struct file* file;
	uint64_t pos;
};

struct directory_desc {
	struct file* dir;
	struct list_node* current;
};

struct dir_entry {
	struct list_node link;
	char name[FILE_NAME];
	struct file file;
};

enum file_flags {
	O_APPEND    = 1 << 0,
	O_CREAT     = 1 << 1,
	O_DIRECTORY = 1 << 2,
	O_TRUNCATE  = 1 << 3
};

void fs_init(void);

struct file_desc* open(const char* pathname, int mode);
uint64_t read(struct file_desc* fd, char* buffer, uint64_t size);
uint64_t write(struct file_desc* fd, const char* buffer, uint64_t size);
void close(struct file_desc* file);

bool mkdir(const char* pathname);
struct directory_desc* opendir(const char* pathname);
struct dir_entry* readdir(struct directory_desc* dir_desc);
void closedir(struct directory_desc* dir_desc);

void ls(void);
