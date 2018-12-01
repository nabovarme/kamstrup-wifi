#include <esp8266.h>
#include "tinyprintf.h"
#include "exception_handler.h"
#include "xtensa/corebits.h"

#define STACK_TRACE_SEC				0x80

#define STACK_TRACE_BUFFER_N		128

//The asm stub saves the Xtensa registers here when a debugging exception happens.
struct XTensa_exception_frame_s saved_regs;

// struct for buffered save log to flash
struct stack_trace_context_t {
	size_t flash_index;
	char buffer[STACK_TRACE_BUFFER_N];
	size_t buffer_index;
} stack_trace_context;

extern void _xtos_set_exception_handler(int cause, void (exhandler)(struct XTensa_exception_frame_s *frame));
extern void ets_wdt_disable();
extern void ets_wdt_enable();

extern void save_extra_sfrs_for_exception();

//Get the value of one of the A registers
static unsigned int getaregval(int reg) {
	if (reg == 0) return saved_regs.a0;
	if (reg == 1) return saved_regs.a1;
	return saved_regs.a[reg-2];
}

static void print_stack(uint32_t start, uint32_t end) {
	uint32_t pos;
	uint32_t *values;
	bool looks_like_stack_frame;
	printf("\nStack dump:\n");
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "\nStack dump:\n");
	stack_trace_append(stack_trace_context.buffer);

	for (pos = start; pos < end; pos += 0x10) {
		values = (uint32_t*)(pos);
		// rough indicator: stack frames usually have SP saved as the second word
		looks_like_stack_frame = (values[2] == pos + 0x10);
		
		printf("%08lx:  %08lx %08lx %08lx %08lx %c\n",
			(long unsigned int) pos, 
			(long unsigned int) values[0], 
			(long unsigned int) values[1],
			(long unsigned int) values[2], 
			(long unsigned int) values[3], 
			(looks_like_stack_frame)?'<':' ');
		tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "%08lx:  %08lx %08lx %08lx %08lx %c\n",
			(long unsigned int) pos, 
			(long unsigned int) values[0], 
			(long unsigned int) values[1],
			(long unsigned int) values[2], 
			(long unsigned int) values[3], 
			(looks_like_stack_frame)?'<':' ');
			stack_trace_append(stack_trace_context.buffer);
	}
	printf("\n");
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "\n");
	stack_trace_append(stack_trace_context.buffer);
	
	stack_trace_last();
}

// Print exception info to console
static void print_reason() {
	int i;
	unsigned int r;
	//register uint32_t sp asm("a1");
	struct XTensa_exception_frame_s *reg = &saved_regs;
	printf("\n\n***** Fatal exception %ld\n", (long int) reg->reason);
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "\n\n***** Fatal exception %ld\n", (long int) reg->reason);
	stack_trace_append(stack_trace_context.buffer);
	
	printf("pc=0x%08lx sp=0x%08lx excvaddr=0x%08lx\n", 
		(long unsigned int) reg->pc, 
		(long unsigned int) reg->a1, 
		(long unsigned int) reg->excvaddr);
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "pc=0x%08lx sp=0x%08lx excvaddr=0x%08lx\n", 
		(long unsigned int) reg->pc, 
		(long unsigned int) reg->a1, 
		(long unsigned int) reg->excvaddr);
	stack_trace_append(stack_trace_context.buffer);
	
	printf("ps=0x%08lx sar=0x%08lx vpri=0x%08lx\n", 
		(long unsigned int) reg->ps, 
		(long unsigned int) reg->sar,
		(long unsigned int) reg->vpri);
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "ps=0x%08lx sar=0x%08lx vpri=0x%08lx\n", 
		(long unsigned int) reg->ps, 
		(long unsigned int) reg->sar,
		(long unsigned int) reg->vpri);
	stack_trace_append(stack_trace_context.buffer);
	
	for (i = 0; i < 16; i++) {
		r = getaregval(i);
		printf("r%02d: 0x%08x=%10d ", i, r, r);
		tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "r%02d: 0x%08x=%10d ", i, r, r);
		stack_trace_append(stack_trace_context.buffer);
		if (i%3 == 2) {
			printf("\n");
			tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "\n");
			stack_trace_append(stack_trace_context.buffer);
		}
	}
	printf("\n");
	tfp_snprintf(stack_trace_context.buffer, STACK_TRACE_BUFFER_N, "\n");
	stack_trace_append(stack_trace_context.buffer);
	//print_stack(reg->pc, sp, 0x3fffffb0);
	print_stack(getaregval(1), 0x3fffffb0);
}

static void exception_handler(struct XTensa_exception_frame_s *frame) {
	// Save the extra registers the Xtensa HAL doesn't save
	save_extra_sfrs_for_exception();
	// Copy registers the Xtensa HAL did save to saved_regs
	memcpy(&saved_regs, frame, 19*4);
	// Credits go to Cesanta for this trick. A1 seems to be destroyed, but because it
	// has a fixed offset from the address of the passed frame, we can recover it.
	// saved_regs.a1=(uint32_t)frame+EXCEPTION_GDB_SP_OFFSET;
	saved_regs.a1=(uint32_t)frame;

	ets_wdt_disable();
	
	spi_flash_erase_sector(STACK_TRACE_SEC);
	spi_flash_erase_sector(STACK_TRACE_SEC + 1);
	spi_flash_erase_sector(STACK_TRACE_SEC + 2);
	spi_flash_erase_sector(STACK_TRACE_SEC + 3);
	
	print_reason();
	
	ets_wdt_enable();

	while(1) {
		// wait for watchdog to bite
	}
}

ICACHE_FLASH_ATTR
void exception_handler_init() {
	unsigned int i;
	int exno[] = {EXCCAUSE_ILLEGAL, EXCCAUSE_SYSCALL, EXCCAUSE_INSTR_ERROR, EXCCAUSE_LOAD_STORE_ERROR,
				  EXCCAUSE_DIVIDE_BY_ZERO, EXCCAUSE_UNALIGNED, EXCCAUSE_INSTR_DATA_ERROR, EXCCAUSE_LOAD_STORE_DATA_ERROR,
				  EXCCAUSE_INSTR_ADDR_ERROR, EXCCAUSE_LOAD_STORE_ADDR_ERROR, EXCCAUSE_INSTR_PROHIBITED,
				  EXCCAUSE_LOAD_PROHIBITED, EXCCAUSE_STORE_PROHIBITED};

	// initialize buffered save log to flash
	memset(stack_trace_context.buffer, 0, STACK_TRACE_BUFFER_N);
	stack_trace_context.flash_index = 0;
	stack_trace_context.buffer_index = 0;

	for (i = 0; i < (sizeof(exno) / sizeof(exno[0])); i++) {
		_xtos_set_exception_handler(exno[i], exception_handler);
	}
}

ICACHE_FLASH_ATTR
void stack_trace_append(char *c) {
	unsigned int len;
	
	len = strlen(c);
	
	if (len + stack_trace_context.buffer_index < STACK_TRACE_BUFFER_N) {
		// fill into buffer
		memcpy(stack_trace_context.buffer + stack_trace_context.buffer_index, c, len);
		stack_trace_context.buffer_index += len;
	}
	else {
		// data longer than buffer size, we need to split writes
		if (len > STACK_TRACE_BUFFER_N) {
			// data enough left to fill buffer
			memcpy(stack_trace_context.buffer + stack_trace_context.buffer_index, c, STACK_TRACE_BUFFER_N - stack_trace_context.buffer_index);
			spi_flash_write((STACK_TRACE_SEC * SPI_FLASH_SEC_SIZE) + (stack_trace_context.flash_index * STACK_TRACE_BUFFER_N),
							(uint32_t *)stack_trace_context.buffer,
							STACK_TRACE_BUFFER_N);
			c += STACK_TRACE_BUFFER_N - stack_trace_context.buffer_index;
			len = strlen(c);
			stack_trace_context.flash_index++;
			stack_trace_context.buffer_index = 0;
			
			// while enough data fill whole buffer
			while (len > STACK_TRACE_BUFFER_N) {
				c += STACK_TRACE_BUFFER_N * stack_trace_context.flash_index + (STACK_TRACE_BUFFER_N - stack_trace_context.buffer_index);
				memcpy(stack_trace_context.buffer, c, STACK_TRACE_BUFFER_N);
				spi_flash_write((STACK_TRACE_SEC * SPI_FLASH_SEC_SIZE) + (stack_trace_context.flash_index * STACK_TRACE_BUFFER_N),
								(uint32_t *)stack_trace_context.buffer,
								STACK_TRACE_BUFFER_N);
				len -= STACK_TRACE_BUFFER_N;
				stack_trace_context.flash_index++;
			}
			
			// last data
			if (len) {
				memset(stack_trace_context.buffer, 0, STACK_TRACE_BUFFER_N);
				memcpy(stack_trace_context.buffer, c, strlen(c));
				stack_trace_context.buffer_index = len;
			}
		}
	}
}

ICACHE_FLASH_ATTR
void stack_trace_last() {
	spi_flash_write((STACK_TRACE_SEC * SPI_FLASH_SEC_SIZE) + (stack_trace_context.flash_index * STACK_TRACE_BUFFER_N),
					(uint32_t *)stack_trace_context.buffer,
					STACK_TRACE_BUFFER_N);
}
