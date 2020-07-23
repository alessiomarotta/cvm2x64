#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef DEBUG
	bool debug = true;
#else
	bool debug = false;
#endif

#define CODE_SIZE 0x1000

#define HALT 0
#define DISPLAY 1
#define PRINT_STACK 2
#define PUSH 10
#define POP 11
#define MOV 12
#define CALL 20
#define RET 21
#define JMP 22
#define JZ 23
#define JPOS 24
#define JNEG 25
#define ADD 30
#define SUB 31
#define MUL 32
#define DIV 33

typedef void (*function)(void);

void display(int n) {
	printf("%d\n", n);
} 

void print_stack(int64_t *sp, int n) {
	for (int i = 3; i < n+3; i++)
		printf("[%d] %d\n", i-3, sp[*sp-i]);
}

int nextInt(FILE *fp) {
	int n;
	int i = 0;
	bool is_comment = false;
	char s[256] = {0};
	char c = getc(fp);
	
	while (!feof(fp)) {
		if (c >= '0' && c <= '9') {
			if (!is_comment)
				s[i++] = c;
		}

		else if (c == ';')
			is_comment = true;

		else if (c == '\n') {
			if (i > 0) {
				sscanf(s, "%d", &n);
				return n; 
			}

			is_comment = false;
		}

		c = getc(fp);
	}
}

int *parse_cvm(char *fname, int *line_count) {
	FILE *fp = fopen(fname, "r");
	*line_count = nextInt(fp);
	int *buffer = calloc(*line_count + 3, sizeof(int));
	int i = 0;

	while (i < *line_count)
		buffer[i++] = nextInt(fp);

	fclose(fp);

	return buffer;
}

void add_code(uint8_t **code_ptr, const char *data, int size) {
	memcpy(*code_ptr, data, size);
	*code_ptr += size;
}

void add_address(uint8_t **code_ptr, void *data) {
	int size = sizeof(uint64_t);

	memcpy(*code_ptr, &data, size);
	*code_ptr += size;
}

void add_immediate(uint8_t **code_ptr, int n) {
	int size = sizeof(int32_t);
	uint8_t tmp[4];

	tmp[3] = (n >> 24 ) & 0xff;
	tmp[2] = (n >> 16 ) & 0xff;
	tmp[1] = (n >> 8 ) & 0xff;
	tmp[0] = n & 0xff;

	memcpy(*code_ptr, tmp, size);

	*code_ptr += size;
}

void add_byte(uint8_t **code_ptr, uint8_t n) {
	int size = sizeof(uint8_t);

	**code_ptr = n;
	*code_ptr += size;
}

void **get_code_offsets(void *code, int *cvm_code, int line_count) {
	void **offsets = calloc(line_count, sizeof(void *));
	int pc = 0;

	*offsets = code + 21;
	code += 21;

	while(pc < line_count) {
		int op = cvm_code[pc];
		int p1 = cvm_code[pc + 1];
		int p2 = cvm_code[pc + 2];

		*(offsets+pc) = code;

		switch(op) {
			case HALT:
				code += 2;
				pc += 1;
				break;
			case DISPLAY:
				if (p1 == 0) code += 15;
				else code += 16;
				pc += 2;
				break;
			case PRINT_STACK:
				code += 21;
				pc += 2;
				break;
			case PUSH:
				if (p1 == 0) code += 3;
				else code += 5;
				pc += 2;
				break;
			case POP:
				if (p1 == 0) code += 3;
				else code += 4;
				pc += 2;
				break;
			case MOV:
				if (p1 == 0) code += 6;
				else code += 7;
				pc += 3;
				break;
			case CALL:
				code += 5;
				pc += 2;
				break;
			case RET:
				code += 1;
				pc += 1;
				break;
			case JMP:
				code += 5;
				pc += 2;
				break;
			case JZ:
				code += 9;
				pc += 2;
				break;
			case JPOS:
				code += 9;
				pc += 2;
				break;
			case JNEG:
				code += 9;
				pc += 2;
				break;
			case ADD:
				if (p1 == 0) code += 3;
				else code += 4;
				if (p2 == 0) code += 2;
				else code += 3;
				pc += 3;
				break;
			case SUB:
				if (p1 == 0) code += 3;
				else code += 4;
				if (p2 == 0) code += 2;
				else code += 3;
				pc += 3;
				break;
			case MUL:
				if (p1 == 0) code += 3;
				else code += 4;
				if (p2 == 0) code += 3;
				else code += 4;
				pc += 3;
				break;
			case DIV:
				if (p1 == 0) code += 5;
				else code += 6;
				if (p2 == 0) code += 2;
				else code += 3;
				pc += 3;
				break;
			default:
				fprintf(stderr, "Encountered illegal instruction\n");
				exit(1);
		}
	}

	return offsets;
}

int main(int argc, char *argv[]) {
	int line_count;
	int *cvm_code = parse_cvm(argv[1], &line_count);
	int pc = 0;

	void *regs = mmap(0, CODE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	void *code = mmap(0, CODE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	uint8_t *code_buffer = calloc(CODE_SIZE, sizeof(uint8_t));
	uint8_t *code_ptr = code_buffer;

	void **code_offsets = get_code_offsets(code, cvm_code, line_count);

	// push rbp
	// mov rbp, rsp
	// sub rsp, 0x4000
	// mov rbx, regs
	add_code(&code_ptr, "\x55\x48\x89\xe5\x48\x81\xec\x00\x40\x00\x00\x48\xbb", 13);
	add_address(&code_ptr, regs);

	while(pc < line_count) {
		int op = cvm_code[pc];
		int p1 = cvm_code[pc + 1];
		int p2 = cvm_code[pc + 2];

		switch(op) {
			case HALT:
				// leave
				// ret
				add_code(&code_ptr, "\xc9\xc3", 2);
				pc += 1;
				break;
			case DISPLAY:
				// mov rdi, [rbx+p1*4]
				// mov rax, display
				// call rax
				if (p1 == 0)
					add_code(&code_ptr, "\x48\x8b\x3b", 3);

				else {
					add_code(&code_ptr, "\x48\x8b\x7b", 3);
					add_byte(&code_ptr, p1*4);
				}

				add_code(&code_ptr, "\x48\xb8", 2);
				add_address(&code_ptr, display);
				add_code(&code_ptr, "\xff\xd0", 2);
				pc += 2;
				break;
			case PRINT_STACK:
				// mov rdi, rbp
				// mov rsi, p1
				// mov rax, print_stack
				// call rax
				add_code(&code_ptr, "\x48\x89\xe7\xbe", 4);
				add_immediate(&code_ptr, p1);
				add_code(&code_ptr, "\x48\xb8", 2);
				add_address(&code_ptr, print_stack);
				add_code(&code_ptr, "\xff\xd0", 2);

				pc += 2;
				break;
			case PUSH:
				// mov eax, [rbx+p1*4]
				// push rax
				if (p1 == 0)
					add_code(&code_ptr, "\x8b\x03", 2);

				else {
					add_code(&code_ptr, "\x8b\x04\x25", 3);
					add_byte(&code_ptr, p1*4);
				}

				add_code(&code_ptr, "\x50", 1);
				pc += 2;
				break;
			case POP:
				// pop rax
				// mov dword [rbx+p1*4], eax
				add_code(&code_ptr, "\x58", 1);

				if (p1 == 0)
					add_code(&code_ptr, "\x89\x03", 2);

				else {
					add_code(&code_ptr, "\x89\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				pc += 2;
				break;
			case MOV:
				// mov dword [rbx+p1*4], p2
				if (p1 == 0)
					add_code(&code_ptr, "\xc7\x03", 2);

				else {
					add_code(&code_ptr, "\xc7\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				add_immediate(&code_ptr, p2);
				pc += 3;
				break;
			case CALL:
				// call code_offsets+p1
				add_code(&code_ptr, "\xe8", 1);
				add_immediate(&code_ptr, *(code_offsets+p1) - *(code_offsets+pc) - 5);
				pc += 2;
				break;
			case RET:
				// ret
				add_code(&code_ptr, "\xc3", 1);
				pc += 1;
				break;
			case JMP:
				// jmp code_offsets+p1
				add_code(&code_ptr, "\xe9", 1);
				add_immediate(&code_ptr, *(code_offsets+p1) - *(code_offsets+pc) - 5);
				pc += 2;
				break;
			case JZ:
				// pop rax
				// test eax, eax
				// je code_offsets+p1
				add_code(&code_ptr, "\x58\x85\xc0\x0f\x84", 5);
				add_immediate(&code_ptr, *(code_offsets+p1) - *(code_offsets+pc) - 9);
				pc += 2;
				break;
			case JPOS:
				// pop rax
				// test eax, eax
				// jg code_offsets+p1
				add_code(&code_ptr, "\x58\\x85\xc0\x0f\x8f", 5);
				add_immediate(&code_ptr, *(code_offsets+p1) - *(code_offsets+pc) - 9);
				pc += 2;
				break;
			case JNEG:
				// pop rax
				// test eax, eax
				// jl code_offsets+p1
				add_code(&code_ptr, "\x58\x85\xc0\x0f\x8c", 5);
				add_immediate(&code_ptr, *(code_offsets+p1) - *(code_offsets+pc) - 9);
				pc += 2;
				break;
			case ADD:
				// mov eax, [rbx+p1*4]
				// add eax, [rbx+p2*4]
				// push rax
				if (p1 == 0)
					add_code(&code_ptr, "\x8b\x03", 2);

				else {
					add_code(&code_ptr, "\x8b\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				if (p2 == 0)
					add_code(&code_ptr, "\x03\x03", 2);

				else {
					add_code(&code_ptr, "\x03\x43", 2);
					add_byte(&code_ptr, p2*4);
				}

				add_code(&code_ptr, "\x50", 1);

				pc += 3;
				break;
			case SUB:
				// mov eax, [rbx+p1*4]
				// add eax, [rbx+p2*4]
				// push rax
				if (p1 == 0)
					add_code(&code_ptr, "\x8b\x03", 2);

				else {
					add_code(&code_ptr, "\x8b\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				if (p2 == 0)
					add_code(&code_ptr, "\x2b\x03", 2);

				else {
					add_code(&code_ptr, "\x2b\x43", 2);
					add_byte(&code_ptr, p2*4);
				}

				add_code(&code_ptr, "\x50", 1);

				pc += 3;
				break;
			case MUL:
				// mov eax, [rbx+p1*4]
				// imul eax, [rbx+p2*4]
				// push rax
				if (p1 == 0)
					add_code(&code_ptr, "\x8b\x03", 2);

				else {
					add_code(&code_ptr, "\x8b\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				if (p2 == 0)
					add_code(&code_ptr, "\x0f\xaf\x03", 3);

				else {
					add_code(&code_ptr, "\x0f\xaf\x43", 3);
					add_byte(&code_ptr, p2*4);
				}

				add_code(&code_ptr, "\x50", 1);

				pc += 3;
				break;
			case DIV:
				// xor edx, edx
				// mov eax, [rbx+p1*4]
				// idiv dword [rbx+p2*4]
				// push rax
				add_code(&code_ptr, "\x31\xd2", 2);

				if (p1 == 0)
					add_code(&code_ptr, "\x8b\x03", 2);

				else {
					add_code(&code_ptr, "\x8b\x43", 2);
					add_byte(&code_ptr, p1*4);
				}

				if (p2 == 0)
					add_code(&code_ptr, "\xf7\x3b", 2);

				else {
					add_code(&code_ptr, "\xf7\x7b", 2);
					add_byte(&code_ptr, p2*4);
				}

				add_code(&code_ptr, "\x50", 1);

				pc += 3;
				break;
		}	
	}

	if (debug) {
		for (int i = 0; i < 128; i++) {
			fprintf(stderr, "%02x ", code_buffer[i]);
		
			if ((i + 1) % 16 == 0)
				fprintf(stderr, "\n");
		}
	}

	memcpy(code, code_buffer, CODE_SIZE);
	mprotect(code, CODE_SIZE, PROT_READ | PROT_EXEC);
	function exec = code;
	exec();

	free(cvm_code);
	free(code_buffer);
	free(code_offsets);
	munmap(regs, CODE_SIZE);
	munmap(code, CODE_SIZE);

	return 0;
}