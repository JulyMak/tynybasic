#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#define NUM_LAB 100
#define LAB_LEN 10
#define SUB_NEST 25
#define PROG_SIZE 10000

#define DELIMITER  1
#define VARIABLE  2
#define NUMBER    3
#define COMMAND   4
#define STRING	  5
#define QUOTE	  6

#define PRINT 1
#define INPUT 2
#define IF    3
#define THEN  4
#define TO    5
#define GOTO  6
#define EOL   7
#define FINISHED  8
#define GOSUB 9
#define RETURN 10
#define END 11
char *prog;  
int variables[26]= {    
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0
};

struct commands { 
	char command[20];
	char tok;
} table[] = { 
	"print", PRINT, 
	"input", INPUT,
	"if", IF,
	"then", THEN,
	"goto", GOTO,
	"gosub", GOSUB,
	"return", RETURN,
	"end", END,
	"", END  
};

char token[80];
char token_type, tok;

struct label {
	char name[LAB_LEN];
	char *p;  
};
struct label label_table[NUM_LAB];
char *find_label(), *gpop();
char *gstack[SUB_NEST];	
int gtos;  
void assignment();
void print(), scan_labels(), find_eol(), exec_goto();
void exec_if(), input();
void gosub(), greturn(), gpush(), label_init();
void error_type(), get_exp(), putback();
void level2(), level3(),level4(), primitive();
void arith();
int load_program(char *p, char *fname), look_up(char *s);
int get_next_label(char *s), iswhite(char c), isdelim(char c);
int find_var(char *s), get_token();
FILE *out;
int main(int argc, char *argv[])
{
	char *p_buf;
	if(argc!= 3) {
		printf("Use format: <executable file>.exe <program file>.txt <output file>.txt\n");
		exit(1);
	}

	if(!(p_buf = (char *) malloc(PROG_SIZE))) {
		error_type(2);
	}

	if(!load_program(p_buf,argv[1])) exit(1);
	if(!(out = fopen(argv[2], "w+"))) return 0;

	prog = p_buf;
	scan_labels(); 
	gtos = 0; 
	do {
		token_type = get_token();
		if(token_type == VARIABLE) {
			putback(); 
			assignment(); 
		}
		else 
			switch(tok) {
			case PRINT:
				print();
				break;
			case GOTO:
				exec_goto();
				break;
			case IF:
				exec_if();
				break;
			case INPUT:
				input();
				break;
			case GOSUB:
				gosub();
				break;
			case RETURN:
				greturn();
				break;
			case END:
				exit(0);
		}
	} while (tok != FINISHED);
	fclose(out);
}

int load_program(char *p, char *fname)
{
	FILE *fp;
	int i = 0;

	if(!(fp = fopen(fname, "rb"))) return 0; 

	i = 0;
	do {
		*p = getc(fp);
		p++; i++;
	} while(!feof(fp) && i < PROG_SIZE);
	*(p-2) = '\0'; 
	fclose(fp);
	return 1;
}

void assignment()
{
	int var, value;
	get_token();
	if(!isalpha(*token)) {
		error_type(0);
	}
	var = toupper(*token) - 'A'; 

	get_token();
	if(*token != '=') {
		error_type(0);
	}

	get_exp(&value);

	variables[var] = value;
}

void print()
{
	int answer;
	int len = 0, spaces;
	char last_delim;

	do {
		get_token(); 
		if(tok == EOL || tok == FINISHED) break;
		if(token_type == QUOTE) { 
			fputs(token,out);
			len += strlen(token); 
			get_token();
		}
		else { 
			putback();
			get_exp(&answer);
			get_token();
			fprintf(out,"%d",answer);
			len += answer;
		}
		
	} while (*token == ';' || *token == ',');
}

void scan_labels()
{
	int addr;
	char *temp;
	label_init();  
	temp = prog;  
	get_token();
	if(token_type == NUMBER) {
		strcpy(label_table[0].name,token);
		label_table[0].p = prog;
	}

	find_eol();
	do {
		get_token();
		if(token_type == NUMBER) {
			addr = get_next_label(token);
			if(addr == -1 || addr == -2) {
				(addr == -1) ? error_type(2):error_type(1);
			}
			strcpy(label_table[addr].name, token);
			label_table[addr].p = prog;  
		}
		if(tok != EOL) find_eol();
	} while(tok != FINISHED);
	prog = temp;  
}

void find_eol()
{
	while(*prog != '\n'  && *prog != '\0') ++prog;
	if(*prog) prog++;
}

int get_next_label(char *s)
{
	int t;
	for(t = 0; t  < NUM_LAB; ++t) {
		if(label_table[t].name[0] == 0) return t;
		if(!strcmp(label_table[t].name, s)) return -2; /* dup */
	}
	return -1;
}

char *find_label(s)
	char *s;
{
	int t;

	for(t = 0; t < NUM_LAB; ++t)
		if(!strcmp(label_table[t].name,s)) return label_table[t].p;
	return '\0'; 
}

void exec_goto()
{
	char *loc;
	get_token(); 
	loc = find_label(token);
	if(loc =='\0')
		error_type(0); 
	else prog = loc;  
}

void label_init()
{
	int t;
	for(t = 0; t < NUM_LAB; ++t) label_table[t].name[0] = '\0';
}

void exec_if()
{
	int x , y, cond;
	char op;
	char op_sec;
	get_exp(&x); 
	get_token(); 
	if(!strchr("=<>", *token)) {
		error_type(0); 
		return;
	} 
	op = *token;
	cond = 0;
	switch(op) {
	case '<':
		get_token();
		if(strchr("=<>", *token)){
			op_sec = *token;
			switch(op_sec)
			{
			case '>':
				get_exp(&y); 
				if(x != y) cond = 1;
				break;
			case '=':
				get_exp(&y); 
				if(x <= y) cond = 1;
				break;
			case '<':
				error_type(0);
				break;
			}
		} else {
			putback();
			get_exp(&y); 
			if(x < y) cond = 1;
			break;
		}
		break;
	case '>':
		get_token();
		if(strchr("=<>", *token)){
			op_sec = *token;
			switch(op_sec)
			{
			case '>':
				error_type(0);
				break;
			case '=':
				get_exp(&y);
				if(x >= y) cond = 1;
				break;
			case '<':
				error_type(0);
				break;
			}
		} else {
			putback();
			get_exp(&y);
			if(x > y) cond = 1;
			break;
		}
		break;
	case '=':
		get_exp(&y);
		if(x == y) cond = 1;
		break;
	}
	if(cond) { 
		get_token();
		if(tok != THEN) {
			error_type(1);
			return;
		}
	}
	else find_eol(); 
}

void input()
{
	char var;
	int i;
	get_token(); 
	if(token_type == QUOTE) {
		fputs(token,out); 
		get_token();
		if(*token != ',') error_type(0);
		get_token();
	}
	else printf("input: "); 
	var = toupper(*token) -'A'; 
	scanf("%d", &i); 
	variables[var] = i; 
}

void gosub()
{
	char *loc;
	get_token();
	loc = find_label(token);
	if(loc =='\0')
		error_type(0); 
	else {
		gpush(prog); 
		prog = loc;  
	}
}

void greturn()
{
	prog = gpop();
}

void gpush(s)
	char *s;
{
	gtos++;

	if(gtos == SUB_NEST) {
		error_type(0);
		return;
	}
	gstack[gtos] = s;
}

char *gpop()
{
	if(gtos == 0) {
		error_type(0);
		return 0;
	}
	return(gstack[gtos--]);
}
void get_exp(result)
	int *result;
{
	get_token();
	if(!*token) {
		error_type(0);
		return;
	}
	level2(result);
	putback(); 
}

void error_type(error)
	int error;
{
	static char *e[]= {
		"WHAT?",
		"HOW?",
		"SORRY?",
	};
	fputs(e[error],out);
	exit(1);
}

int get_token()
{
	char *temp;
	token_type = 0; tok = 0;
	temp = token;
	if(*prog == '\0') { /* end of file */
		*token = 0;
		tok = FINISHED;
		return(token_type = DELIMITER);
	}
	while(iswhite(*prog)) ++prog;  /* skip over white space */
	if(*prog == '\r') { /* crlf */
		++prog; ++prog;
		tok = EOL; *token = '\r';
		token[1] = '\n'; token[2] = 0;
		return (token_type = DELIMITER);
	}
	if(strchr("+-*^/%=;(),><", *prog)){ /* delimiter */
		*temp = *prog;
		prog++; /* advance to next position */
		temp++;
		*temp = 0;
		return (token_type = DELIMITER);
	}
	if(*prog == '"') { /* quoted string */
		prog++;
		while(*prog != '"' && *prog != '\r') *temp++ = *prog++;
		if(*prog == '\r') error_type(0);
		prog++;
		*temp = 0;
		return(token_type = QUOTE);
	}
	if(isdigit(*prog)) { 
		while(!isdelim(*prog)) *temp++ = *prog++;
		*temp = '\0';
		return(token_type = NUMBER);
	}
	if(isalpha(*prog)) { 
		while(!isdelim(*prog)) *temp++ = *prog++;
		token_type = STRING;
	}
	*temp = '\0';
	if(token_type == STRING) {
		tok = look_up(token); 
		if(!tok) token_type = VARIABLE;
		else token_type = COMMAND; 
	}
	return token_type;
}

void putback()
{
	char *t;
	t = token;
	for(; *t; t++) prog--;
}
int look_up(char *s)
{
	int i;
	char *p;
	p = s;
	while(*p){ 
		*p = tolower(*p); 
		p++; 
	}
	for(i = 0; *table[i].command; i++)
		if(!strcmp(table[i].command, s)) return table[i].tok;
	return 0; 
}

int isdelim(char c)
{
	if(strchr(" ;,+-<>/*%^=()", c) || c == 9 || c == '\r' || c == 0)
		return 1;
	return 0;
}

int iswhite(char c)
{
	if(c == ' ' || c == '\t') return 1;
	else return 0;
}

void level2(result)
	int *result;
{
	char  op;
	int hold;
	level3(result);
	while((op = *token) == '+' || op == '-') {
		get_token();
		level3(&hold);
		arith(op, result, &hold);
	}
}

void level3(result)
	int *result;
{
	char  op;
	int hold;

	level4(result);
	while((op = *token) == '*' || op == '/') {
		get_token();
		level4(&hold);
		arith(op, result, &hold);
	}
}

void level4(result)
	int *result;
{
	if((*token == '(') && (token_type == DELIMITER)) {
		get_token();
		level2(result);
		if(*token != ')')
			error_type(0);
		get_token();
	}
	else
		primitive(result);
}

void primitive(result)
	int *result;
{

	switch(token_type) {
	case VARIABLE:
		*result = find_var(token);
		get_token();
		return;
	case NUMBER:
		*result = atoi(token);
		get_token();
		return;
	default:
		error_type(0);
	}
}

void arith(o, r, h)
	char o;
int *r, *h;
{
	int t;

	switch(o) {
	case '-':
		*r = *r - *h;
		if(*r > 32767 || *r < -32767){
			error_type(1);
		}
		break;
	case '+':
		*r = *r + *h;
		if(*r > 32767 || *r < -32767){
			error_type(1);
		}
		break;
	case '*':
		*r = *r * *h;
		if(*r > 32767 || *r < -32767){
			error_type(1);
		}
		break;
	case '/':
		*r = (*r)/(*h);
		if(*r > 32767 || *r < -32767){
			error_type(1);
		}
		break;
	}
}

int find_var(char *s)
{
	if(!isalpha(*s)){
		error_type(0); 
		return 0;
	}
	return variables[toupper(*token)-'A'];
} 