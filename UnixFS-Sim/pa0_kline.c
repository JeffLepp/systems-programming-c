#include <stdio.h>            
#include <stdlib.h>     
#include <string.h>
#include <sys/types.h>

// change this typedef
typedef void (*cmd_run)(char *arg);

// forward-declare your commands with (char *) args
void mkdir_cmd(char *arg);
void rmdir_cmd(char *arg);
void cd_cmd(char *arg);
void ls_cmd(char *arg);
void pwd_cmd(char *arg);
void creat_cmd(char *arg);
void rm_cmd(char *arg);
void quit_cmd(char *arg);

typedef struct { char *name; cmd_run fn; } Command;

Command commands[] = {
  {"mkdir",  mkdir_cmd},
  {"rmdir",  rmdir_cmd},
  {"cd",     cd_cmd},
  {"ls",     ls_cmd},
  {"pwd",    pwd_cmd},
  {"creat",  creat_cmd},
  {"rm",     rm_cmd},
  {"quit",   quit_cmd},
};

//// NODES ////

typedef struct node {
	char  name[64];       // node's name string
	char  type;			  // D (for directoris) or F (for files)
	struct node *child, *sibling, *parent;
} NODE;

NODE *root; 
NODE *cwd;

void initialize(void) {
	root = (NODE *)malloc(sizeof(NODE));
	strcpy(root->name, "/");
	root->parent = root;
	root->sibling = 0;
	root->child = 0;
	root->type = 'D';
	cwd = root;
	// other steps as needed
	
	printf("Filesystem initialized!\n");
}

NODE *newNode(char *name, char type, NODE *parent) {
	NODE *new_node = malloc (sizeof(*new_node));
	strncpy(new_node->name, name, sizeof(new_node->name)-1);
	new_node->type = type;
	new_node->child = NULL;
    new_node->sibling = NULL;
	new_node->parent = parent;

	return new_node;
}

//// TREE ////

void add_child(NODE *parent, NODE* child_to_add) {
	if (parent->child == NULL) {
		parent->child = child_to_add;
	} else {
		NODE *current_child = parent->child;
		while (current_child->sibling) {
			current_child = current_child->sibling;
		}
		current_child->sibling = child_to_add;
	}
	child_to_add->parent = parent;
}

NODE *find_child(NODE *parent, char *name) {
	NODE *current_child = parent->child;
	while (current_child) {
		if (strcmp(current_child->name, name) == 0) {
			return current_child;
		}
		current_child = current_child->sibling;
	}
	return NULL;
}

void print_tree_logic(NODE *current_node, int depth) {
	for (int i = 0; i < depth; i++) {
		printf("--");
	}
	printf("%s (%c)\n", current_node->name, current_node->type);
	NODE *child = current_node->child;
	while (child) {
		print_tree_logic(child, depth + 1);
		child = child->sibling;
	}
}

void print_tree(void) {
	print_tree_logic(root, 0);
}

// CHECK IF THIS IMPL WORKING
NODE *resolve_path(char *path_in) {
    if (!path_in || !*path_in) return cwd;      
    if (strcmp(path_in, "/") == 0) return root;

    NODE *current = (path_in[0] == '/') ? root : cwd;

    char *temp_str = strdup(path_in);
    char *p = temp_str;
    if (p[0] == '/') p++;   

    for (char *token = strtok(p, "/"); token; token = strtok(NULL, "/")) {
        if (strcmp(token, ".") == 0) {
            continue;
        } else if (strcmp(token, "..") == 0) {
            if (current != root) current = current->parent;
            continue;
        } else {
            NODE *next = find_child(current, token);
            if (!next) { free(temp_str); return NULL; }
            current = next;
        }
    }
    free(temp_str);
    return current;
}

//// COMMANDS ////

void quit_cmd(char *arg) {
	(void)arg;
	printf("Exiting filesystem. Goodbye!\n");
	exit(0);
}

void mkdir_cmd(char *arg) {
	if (!arg || !*arg) {
		printf("Usage: mkdir name or mkdir path/to/name \n");
		return;
	}

	char *path = strdup(arg);
	size_t  length = strlen(path);

	if (find_child(cwd, path)) {
		printf("mkdir: cannot create directory '%s': File exists\n", path);
		free(path);
		return;
	}

	while(length > 1 && path[length -1] == '/') {
		path[length - 1] = '\0';
		length--;
	}

	char *last_slash = strrchr(path, '/');
	NODE *parent = NULL;

	if (last_slash) {
		if (last_slash == path) {
			parent = root;
		} else {
			*last_slash = '\0';
			parent = resolve_path(path);
		}
		last_slash++;
	} else {
		parent = cwd;
		last_slash = path;
	}

	NODE *new_dir = newNode(last_slash, 'D', parent);
	add_child(parent, new_dir);
	free(path);
}

void unlinking_removed_directory(NODE *target) {
	NODE *parent = target->parent;
	if (parent->child == target) {
		parent->child = target->sibling;
	} else {
		NODE *current = parent->child;
		while (current && current->sibling != target) {
			current = current->sibling;
		}
		if (current) {
			current->sibling = target->sibling;
		}
	}
	free(target);
}

void rmdir_cmd(char *arg) {
	if (!arg || !*arg) {
        printf("usage: rmdir path\n");
        return;
    }

	char *copy = strdup(arg);
	char *path = strtok(copy, " \t");

	size_t length = strlen(path);
	while (length > 1 && path[length - 1] == '/') {
		path[length - 1] = '\0';
		length--;
	}
	NODE *target = resolve_path(path);

	if (!target) {
		printf("rmdir: failed to remove '%s': No such file or directory\n", path);
		free(copy);
		return;
	}
	if (target == cwd) {
		printf("rmdir: failed to remove '%s': Cannot remove current working directory\n", path);
		free(copy);
		return;
	}
	if (target->type != 'D') {
        printf("rmdir: failed to remove '%s': Not a directory\n", path);
        free(copy);
        return;
    }
    if (target->child != NULL) {
        printf("rmdir: failed to remove '%s': Directory not empty\n", path);
        free(copy);
        return;
    }

	NODE *parent = target->parent;
	unlinking_removed_directory(target);
	free(copy);

}

void cd_cmd(char *dir) {
	NODE *dest = resolve_path((dir && *dir) ? dir : "/");
	if (!dest) {
		printf("cd: no such file or directory: %s\n", (dir && *dir) ? dir : "/");
		return;
	}
    if (dest->type == 'F') {
        printf("cd: not a directory: %s\n", (dir && *dir) ? dir : "/");
        return;
    }
    cwd = dest;
}

void ls_cmd(char *arg) {
	NODE *current_node = cwd->child;
	if (current_node == NULL) {
		printf("No files or directories found.\n");
		return;
	}
	while (current_node) {
		printf("%s (%c)\n", current_node->name, current_node->type);
		current_node = current_node->sibling;
	}
}

void pwd_cmd(char *arg) {
	printf("Function still being made\n");
	printf("Current working directory: ");
	(void)arg;
	if (cwd == root) {
		printf("/\n");
		return;
	}
	NODE *current = cwd;
	char path[1024] = "";
	path[0] = '\0';
	while (current != root) {
		char temp[1024];
		int n = snprintf(temp, sizeof(temp), "/%s%s", current->name, path);
		strcpy(path, temp);
		current = current->parent;
	}
	printf("%s\n", path[0] ? path : "/");

}

void creat_cmd(char *arg) {
	if (!arg || !*arg) {
		printf("Usage: creat name or creat path/to/name \n");
		return;
	}

	char *path = strdup(arg);
	size_t  length = strlen(path);

	if (find_child(cwd, path)) {
		printf("creat: cannot create file '%s': File exists\n", path);
		free(path);
		return;
	}

	while(length > 1 && path[length -1] == '/') {
		path[length - 1] = '\0';
		length--;
	}

	char *last_slash = strrchr(path, '/');
	NODE *parent = NULL;

	if (last_slash) {
		if (last_slash == path) {
			parent = root;
		} else {
			*last_slash = '\0';
			parent = resolve_path(path);
		}
		last_slash++;
	} else {
		parent = cwd;
		last_slash = path;
	}

	NODE *new_file = newNode(last_slash, 'F', parent);
	add_child(parent, new_file);
	free(path);
}

void rm_cmd(char *arg) {
	if (!arg || !*arg) {
        printf("usage: rmdir path\n");
        return;
    }

	char *copy = strdup(arg);
	char *path = strtok(copy, " \t");

	size_t length = strlen(path);
	while (length > 1 && path[length - 1] == '/') {
		path[length - 1] = '\0';
		length--;
	}

	NODE *target = resolve_path(path);

	if (target->type == 'D') {
		printf("rm: cannot remove '%s': Is a directory\n", path);
		free(copy);
		return;
	}

	if (!target) {
		printf("rm: failed to remove '%s': No such file or directory\n", path);
		free(copy);
		return;
	}

	if (target->child != NULL) {
        printf("rmdir: failed to remove '%s': Directory not empty\n", path);
        free(copy);
        return;
    }

	NODE *parent = target->parent;
	unlinking_removed_directory(target);
	free(copy);

}

//// GLOBAL ////

// Old Logic, todo remove
// typedef void (*cmd_run)(void);
// char *cmd[] = {"mkdir", "rmdir", "cd", "ls", "pwd", "creat", "rm", "save", "reload", "quit"};
// cmd_run functions[] = {mkdir, rmdir, cd, ls, pwd, creat, rm, save, reload, quit};

//// MAIN ////

// functions: add_child, newNode, find_child

int main() {
	initialize();

	char *user_input_line = NULL;
	size_t user_input_length = 0;

	// NODE *a = newNode("a", 'D', root);
	// add_child(root, a);
	// NODE *b = newNode("b", 'D', root);
	// add_child(root, b);
	// NODE *c = newNode("c", 'D', root);
	// add_child(root, c);

	// NODE *a_b = newNode("b", 'D', a);
	// add_child(a, a_b);
	// NODE *a_c = newNode("c", 'F', a);
	// add_child(a, a_c);

	// NODE *b_d = newNode("d", 'D', b);
	// add_child(b, b_d);
	// NODE *b_e = newNode("e", 'F', b);
	// add_child(b, b_e);
	// NODE *b_f = newNode("f", 'F', b);
	// add_child(b, b_f);

	// NODE *c_d = newNode("d", 'D', c);
	// add_child(c, c_d);
	// NODE *c_g = newNode("g", 'F', c);
	// add_child(c, c_g);

	// NODE *a_b_h = newNode("h", 'D', a_b);
	// add_child(a_b, a_b_h);
	// NODE *a_b_i = newNode("i", 'F', a_b);
	// add_child(a_b, a_b_i);

	// NODE *b_d_j = newNode("j", 'F', b_d);
	// add_child(b_d, b_d_j);

	// NODE *a_b_h_k = newNode("k", 'F', a_b_h);
	// add_child(a_b_h, a_b_h_k);

	print_tree();

	while(1) {
		printf("Enter command (Enter Ctrl+C to exit): ");
		
		ssize_t user_input = getline(&user_input_line, &user_input_length, stdin);
		if (user_input != -1){
			user_input_line[strcspn(user_input_line, "\n")] = '\0';

			int command_found_flag = 0;

			char *command_token = strtok(user_input_line, " \t");
			char *arguement_token = strtok(NULL, "");

			size_t command_length = sizeof(commands) / sizeof(commands[0]);
			for (size_t i = 0; i < command_length; i++) {

				if (strcmp(command_token, commands[i].name) == 0) {
					command_found_flag = 1;
					commands[i].fn(arguement_token);
					break;
				}
			}
			if (command_found_flag == 0) {
				printf("The command %s could not be found. \n", user_input_line);
			}
		}
	}
}

