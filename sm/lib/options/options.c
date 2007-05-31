#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>

#include <libgen.h>

/* Cavillo (non impatta la portabilit�. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include "options.h"

/** Our version of strdup. */
char * strdup_(const char *s) {
	size_t len = strlen(s) + 1; /* null byte */
	char * t = (char*) malloc(len);
	memcpy(t,s,len);
	return t;
}

/** Return 1 if ok. */
int get_int(int*p, const char*s) {
	int value;
	errno = 0;
	value = strtol(s, (char **)NULL, 10);
	if(0 == errno) {
		*p = value;
		return 1;
	} else return 0;
}

/** Return 1 if ok. */
int get_double(double*p, const char*s) {
	char *endptr;
	*p = strtod(s, &endptr);
	return endptr != s;
}

int options_tolerant = 0;

int options_parse_args(struct option*ops, int argc, const char* argv[]) {
	int i;
	for (i=1; i<argc; i++) {
		const char * name = argv[i];
		while(*name == '-') name++;
		
		if(!strcmp("config_dump", name)) {
			options_dump(ops, stdout, 0);
			exit(1);
		}
		
		if(!strcmp("config", name)) {
			if(i>=argc-1) {
				fprintf(stderr, "Please specify config file.\n");
				if(!options_tolerant) return 0;
			}
			if(!options_parse_file(ops, ".", argv[i+1])) {
				if(!options_tolerant) return 0;
			}
			i++;
			continue;
		}
		
		struct option * o;
		if(0 == (o =options_find(ops, name)) ) {
			fprintf(stderr, "Option '%s' not found.\n", name);
			if(!options_tolerant) return 0;
		}

		options_set_passed(o);
		if(options_requires_argument(o)) {
			if(i>=argc-1) {
				fprintf(stderr, "Argument %s needs value.\n", o->name);
				if(!options_tolerant) return 0;
			}
			if(!options_set(o, argv[i+1])) {
				if(!options_tolerant) return 0;
			}
			i++;
		} 

	} /* for */
	
	return 1;
}



/** Flags the option as passed */
void options_set_passed(struct option*o) {
	if(o->set_pointer) 
		*(o->set_pointer) = 1;
}

/** Returns true if the option needs an argument */
int options_requires_argument(struct option*o) {
	return o->value_pointer != 0;
}


int options_parse_stream(struct option*ops, const char*pwd, FILE*file) {
	#define MAX_LINE_LENGTH 10000
	char linesto[MAX_LINE_LENGTH];
	while(fgets(linesto, MAX_LINE_LENGTH-1, file)) {
		char *line=linesto; while(*line) { if(*line=='\n') *line=0; line++; }
		line = linesto;
		while(isspace(*line)) line++;
		if(*line == '#') continue;
		if(*line == '<') { line++;
			while(isspace(*line)) line++;
			if(!options_parse_file(ops, pwd, line)) {
				if(!options_tolerant) return 0;
			}
			continue;
		}
		if(!*line) continue;
		const char * name = line;
		while(!isspace(*line)) line++;
		const char * value;
		if(*line == 0) value = ""; else {
			*line = 0; line++;
			while(isspace(*line)) line++;
			value = line;
		}
				
		if(!options_try_pair(ops, name, value) && !options_tolerant) {
			return 0;
		}
	}
	return 1;
}

int options_parse_file(struct option*ops, const char*pwd, const char*filename) {
	char concat[PATH_MAX*2+1];
	strcpy(concat, pwd);
	strcat(concat, "/");
	strcat(concat, filename);
	char resolved[PATH_MAX+1];
	if(!realpath(concat, resolved)) {
		fprintf(stderr, "Could not resolve '%s' ('%s').\n", concat, resolved); 
		return 0;
	}

	const char * newdir = dirname(resolved);
	if(!newdir) {
		fprintf(stderr, "Could not get dirname for '%s'.\n",  resolved); 
		return 0;
	}
	
	FILE * file;
	file = fopen(resolved,"r");
	if(file==NULL) {
		fprintf(stderr, "Could not open '%s': %s.\n", resolved, strerror(errno)); 
		return 0;
	}
	
	return options_parse_stream(ops, newdir, file);
}


/** Finds an option in the array. Returns 0 if not found. */
struct option * options_find(struct option*ops, const char * name) {
	int j;
	for(j=0;options_valid(ops+j);j++) 
		if(!strcmp(name,ops[j].name)) 
		return ops+j;
	
	return 0;
}

int options_try_pair(struct option*ops, const char*name, const char*value) {
	struct option* o;
	if(0 == (o = options_find(ops, name))) {
		/* error, option does not exist */
		fprintf(stderr, "Option '%s' does not exist.\n", name);
		return 0;
	}
	return options_set(o, value);
}

int options_valid(struct option*o) {
	return o->name != 0;
}

int options_set(struct option*o, const char*value) {
	switch(o->type) {	
		case(OPTION_INT): {
			int * value_pointer = (int*) o->value_pointer;
			int ok = get_int(value_pointer, value);
			if(!ok) {
				fprintf(stderr, "Could not parse int: '%s' = '%s'.\n", o->name, value);
				return 0;
			}
			return 1;
		}

		case(OPTION_STRING): {
			char** value_pointer = (char**) o->value_pointer;
			*value_pointer = (char*) strdup_(value);
/*			fprintf(stderr, 
				"String %s, value_pointer=%p void pointer=%p *value_pointer=%p result=%s\n"
			 ,argv[i+1], value_pointer, o->op[j].value_pointer, *value_pointer,
			 *value_pointer);*/
			return 1;
		}

		case(OPTION_DOUBLE): {
			double * value_pointer = (double*) o->value_pointer;
			int ok = get_double(value_pointer, value);
			if(!ok) {
				fprintf(stderr, "Could not parse double: '%s' = '%s'.\n", o->name, value);
				return 0;
			}
			return 1;
		}
		
		default: {
			/* XXX ERROR */
			fprintf(stderr, "Could not parse type %d: '%s' = '%s'.\n", o->type, o->name, value);
			return 0;
		}
	}
}

const char*options_value_as_string(struct option*o);

void display_table(FILE*f, const char**table, int rows, int columns, int padding) {
/*	int *col_size = malloc(sizeof(int)*columns);*/
	int col_size[columns];
	
	int i,j;
	for(j=0;j<columns;j++)  {
		col_size[j]=0;
		for(i=0;i<rows;i++) {
			const char * s = table[j+i*columns];
			col_size[j] = MAX(col_size[j], (int) strlen(s));
		}
		col_size[j] += padding;
	}
	
	for(i=0;i<rows;i++) {
		for(j=0;j<columns;j++)  {
			const char * s = table[j+i*columns];
			fprintf(f, "%s%*s", s, (int)(col_size[j]-strlen(s)), "");
		}
		fprintf(f, "\n");
	}
	
/*	free(col_size);*/
}

void options_dump(struct option * options, FILE*f, int write_desc) {
	int n; for (n=0;options_valid(options+n);n++);

	const char**table = malloc(sizeof(char*)*n*3);

	int i;
	for (i=0;i<n;i++) {
		table[i*3 +0] = strdup_(options[i].name);
		table[i*3 +1] = strdup_(options_value_as_string(options+i));
		table[i*3 +2] = write_desc ? strdup_(options[i].desc) : strdup_(""); 
	}

	display_table(f, table, n, 3, 2);

	int a; for(a=0;a<n*3;a++) free((void*)table[a]);
	free((void*)table);
}

void options_print_help(struct option * options, FILE*f) {
	options_dump(options, f, 1);
}

char options_value_as_string_buf[1000];
const char*options_value_as_string(struct option*o) {
	if(!o->value_pointer) {
		return "NULL";
	}
	
	switch(o->type) {
		case(OPTION_INT): {
			int * value_pointer = (int*) o->value_pointer;
			sprintf(options_value_as_string_buf, "%d", *value_pointer);
			break;
		}

		case(OPTION_STRING): {
			char** value_pointer = (char**) o->value_pointer;
			sprintf(options_value_as_string_buf, "%s", *value_pointer);
			break;
		}
		
		case(OPTION_DOUBLE): {
			double * value_pointer = (double*) o->value_pointer;
			sprintf(options_value_as_string_buf, "%g", *value_pointer);
			break;
		}
		default: 
		strcpy(options_value_as_string_buf, "?");
	} /* switch */	
	
	return options_value_as_string_buf;
}
