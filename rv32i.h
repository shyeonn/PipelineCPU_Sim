/* **************************************
 * Module: top design of rv32i pipelined processor
 *
 * Author: Sanghyeon Park
 *
 * **************************************
 */

// headers
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// defines
#define REG_WIDTH 32
#define IMEM_DEPTH 1024
#define DMEM_DEPTH 1024
#define WORD_SIZE 4
#define BYTE_BIT 8

// Opcode
#define I_L_TYPE 0b0000011
#define I_R_TYPE 0b0010011
#define I_J_TYPE 0b1100111
#define S_TYPE 0b0100011
#define R_TYPE 0b0110011
#define SB_TYPE 0b1100011
#define U_LU_TYPE 0b0110111
#define U_AU_TYPE 0b0010111
#define UJ_TYPE 0b1101111

// Func3(Loads)
#define LB 0b000
#define LH 0b001
#define LW 0b010
#define LBU 0b100
#define LHU 0b101

// Func3(Stores)
#define SB 0b000
#define SH 0b001
#define SW 0b010

// Func3(Arithmetic & Shifts)
#define F3_SL 0b001
#define F3_SR 0b101
#define F3_ADD_SUB 0b000
#define F3_XOR 0b100
#define F3_OR 0b110
#define F3_AND 0b111
#define F3_SLT 0b010
#define F3_SLTU 0b011

// Func3(Branches)
#define F3_BEQ 0b000
#define F3_BNE 0b001
#define F3_BLT 0b100
#define F3_BGE 0b101
#define F3_BLTU 0b110
#define F3_BGEU 0b111

// Alu control
#define C_AND 0b0000
#define C_OR 0b0001
#define C_XOR 0b0011
#define C_SL 0b1000
#define C_SR 0b1010
#define C_SRA 0b1011
#define C_SUB 0b0110
#define C_ADD 0b0010


// configs
#define CLK_NUM 45

// Register
enum REG {
  READ = 0,
  WRITE
};

// structures
struct imem_input_t {
	uint32_t addr;
};

struct imem_output_t {
	uint32_t dout;
};

struct regfile_input_t {
	uint8_t rs1;
	uint8_t rs2;
	uint8_t rd;
	uint32_t rd_din;
};

struct regfile_output_t {
	uint32_t rs1_dout;
	uint32_t rs2_dout;
};

struct alu_input_t {
	uint32_t in1;
	uint32_t in2;
	uint8_t alu_control;
};

struct alu_output_t {
	uint32_t result;
	uint8_t zero;
	uint8_t sign;
	uint8_t ucmp;
};

struct dmem_input_t {
  uint32_t addr;
  uint32_t din;
  uint8_t func3;
  uint8_t mem_write;
  uint8_t mem_read;
};

struct dmem_output_t {
  uint32_t dout;
};

// structures for pipeline registers
struct pipe_if_id_t {

};

struct pipe_id_ex_t {
	
};

struct pipe_ex_mem_t {

};

struct pipe_mem_wb_t {

};
