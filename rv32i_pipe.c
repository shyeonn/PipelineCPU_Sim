/* **************************************
 * Module: top design of rv32i pipelined processor
 *
 * Author:
 *
 * **************************************
 */

#include "rv32i.h"

int main (int argc, char *argv[]) {

	// get input arguments
	FILE *f_imem, *f_dmem;
	if (argc < 3) {
		printf("usage: %s imem_data_file dmem_data_file\n", argv[0]);
		exit(1);
	}

	if ( (f_imem = fopen(argv[1], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[1]);
		exit(1);
	}
	if ( (f_dmem = fopen(argv[2], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[2]);
		exit(1);
	}

	// memory data (global)
	uint32_t *reg_data;
	uint32_t *imem_data;
	uint32_t *dmem_data;

	reg_data = (uint32_t*)malloc(32*sizeof(uint32_t));
	imem_data = (uint32_t*)malloc(IMEM_DEPTH*sizeof(uint32_t));
	dmem_data = (uint32_t*)malloc(DMEM_DEPTH*sizeof(uint32_t));

	// initialize memory data
	int i, j, k;
	for (i = 0; i < 32; i++) reg_data[i] = 0;
	for (i = 0; i < IMEM_DEPTH; i++) imem_data[i] = 0;
	for (i = 0; i < DMEM_DEPTH; i++) dmem_data[i] = 0;

	uint32_t d, buf;
	i = 0;
	printf("\n*** Reading %s ***\n", argv[1]);
	while (fscanf(f_imem, "%1d", &buf) != EOF) {
		d = buf << 31;
		for (k = 30; k >= 0; k--) {
			if (fscanf(f_imem, "%1d", &buf) != EOF) {
				d |= buf << k;
			} else {
				printf("Incorrect format!!\n");
				exit(1);
			}
		}
		imem_data[i] = d;
		printf("imem[%03d]: %08X\n", i, imem_data[i]);
		i++;
	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[2]);
	while (fscanf(f_dmem, "%8x", &buf) != EOF) {
		dmem_data[i] = buf;
		printf("dmem[%03d]: %08X\n", i, dmem_data[i]);
		i++;
	}

	fclose(f_imem);
	fclose(f_dmem);

	// processor model
	uint32_t pc_curr, pc_next;	// program counter

	// IF
	struct imem_input_t imem_in;
	struct imem_output_t imem_out;

	// ID
	struct regfile_input_t regfile_in;
	struct regfile_output_t regfile_out;
	uint8_t opcode;
	uint8_t func3;
	uint8_t func7;
	uint32_t imm;

	// EX
	struct alu_input_t alu_in;
	struct alu_output_t alu_out;

	// MEM
	struct dmem_input_t dmem_in;
	struct dmem_output_t dmem_out;

	// pipeline registers
	struct pipe_if_id_t id;
	struct pipe_id_ex_t ex;
	struct pipe_ex_mem_t mem;
	struct pipe_mem_wb_t wb;

	uint32_t cc = 2;	// clock count
	while (cc < CLK_NUM) {
		printf("\n*** CLK : %d ***\n", cc);

		// Program counter
		pc_next = pc_curr + 4;

		if(opcode == SB_TYPE){
			uint8_t pc_next_sel = 0;
			uint32_t r1 = regfile_out.rs1_dout;
			uint32_t r2 = regfile_out.rs2_dout;

			switch(func3){
				case F3_BEQ:
					pc_next_sel = alu_out.zero ? 1 : 0;
					break;
				case F3_BNE:
					pc_next_sel = alu_out.zero ? 0 : 1;
					break;
				case F3_BLT:
					pc_next_sel = (!alu_out.zero && alu_out.sign) ? 1 : 0;
					break;
				case F3_BGE:
					pc_next_sel = (alu_out.zero || !alu_out.sign) ? 1 : 0;
					break;
				case F3_BLTU:
					pc_next_sel = (!alu_out.zero && alu_out.ucmp) ? 1 : 0;
					break;
				case F3_BGEU:
					pc_next_sel = (alu_out.zero || !alu_out.ucmp) ? 1 : 0;
					break;
			}
			if(pc_next_sel){
				pc_next = pc_curr + (int32_t)imm;
				D_PRINTF("PC", "Take branch");
			}
		}
		else if(opcode == UJ_TYPE)
			pc_next = pc_curr + (int32_t)imm;
		else if(opcode == I_J_TYPE)
			pc_next = alu_out.result;


		// write back stage
		if(!(opcode == SB_TYPE || opcode == S_TYPE)){
			if(opcode == UJ_TYPE || opcode == I_J_TYPE)
				regfile_in.rd_din = pc_curr + 4;
			else if(opcode == U_LU_TYPE)
				regfile_in.rd_din = imm;
			else if(opcode == U_AU_TYPE)
				regfile_in.rd_din = pc_curr + imm;
			else if(opcode == R_TYPE || opcode == I_R_TYPE){
				if(func3 == F3_SLT){
					regfile_in.rd_din = alu_out.sign ? 1 : 0;
				}
				else if(func3 == F3_SLTU){
					regfile_in.rd_din = alu_out.ucmp ? 1 : 0;
				}
				else
					regfile_in.rd_din = alu_out.result;
			}
			else if(opcode == I_L_TYPE){
				regfile_in.rd_din = dmem_out.dout;
			}
			else
				regfile_in.rd_din = alu_out.result;

			D_PRINTF("WB", "[I]rd - %d",regfile_in.rd);
			D_PRINTF("WB", "[I]rd_din - %d",regfile_in.rd_din);

			regfile_out = regfile(regfile_in, reg_data, WRITE);
		}


		// memory stage
		if(opcode == S_TYPE || opcode == I_L_TYPE || opcode == I_J_TYPE){
			dmem_in.addr = alu_out.result;
			D_PRINTF("MEM", "[I]addr - %x", dmem_in.addr);
			dmem_in.din = regfile_out.rs2_dout;
			D_PRINTF("MEM", "[I]din - %x", dmem_in.din);
			dmem_in.func3 = func3;
			D_PRINTF("MEM", "[I]func - %x", dmem_in.func3);

			if(opcode == S_TYPE){
				dmem_in.mem_write = 1;
				dmem_in.mem_read = 0;
			}
			else if(opcode == I_L_TYPE){
				dmem_in.mem_read = 1;
				dmem_in.mem_write = 0;
			}
			else{
				dmem_in.mem_read = 0;
				dmem_in.mem_write = 0;
			}

			dmem_out = dmem(dmem_in, dmem_data);
		}


		// execution stage
		alu_in.alu_control = alu_control_gen(opcode, func3, func7);

		D_PRINTF("EX", "[I]in1 - %d", alu_in.in1);
		D_PRINTF("EX", "[I]in2 - %d", (int32_t) alu_in.in2);
		D_PRINTF("EX", "[I]alu_cont - %x", alu_in.alu_control);

		alu_out = alu(alu_in);

		D_PRINTF("EX", "[I]result - %d", alu_out.result);
		D_PRINTF("EX", "[I]zero - %d", alu_out.zero);
		D_PRINTF("EX", "[I]sign - %d", alu_out.sign);


		// instruction decode stage
		opcode = imem_out.dout & 0x7F;

		if (!(opcode == U_LU_TYPE || opcode == U_AU_TYPE ||opcode == UJ_TYPE))
			func3 = (imem_out.dout >> 12) & 0x7;
		if (opcode == R_TYPE)
			func7 = (imem_out.dout >> 25) & 0x7F;

		regfile_in.rs1 = (imem_out.dout >> 15) & 0x1F;
		D_PRINTF("ID", "[I]rs1 - %d", regfile_in.rs1);
		regfile_in.rd = (imem_out.dout >> 7) & 0x1F;
		D_PRINTF("ID", "[I]rd - %d", regfile_in.rd);

		if (opcode == SB_TYPE || opcode == R_TYPE || opcode == S_TYPE){
			regfile_in.rs2 = (imem_out.dout >> 20) & 0x1F;
			D_PRINTF("ID", "[I]rs2 - %d", regfile_in.rs2);
		}

		regfile_out = regfile(regfile_in, reg_data, READ);

		D_PRINTF("ID", "[O]rs1_dout - %d", regfile_out.rs1_dout);
		if (opcode == SB_TYPE || opcode == R_TYPE || opcode == S_TYPE)
		D_PRINTF("ID", "[O]rs2_dout - %d", regfile_out.rs2_dout);
		
		alu_in.in1 = regfile_out.rs1_dout;

		//Immediate generation
		if (opcode == I_L_TYPE || opcode == I_R_TYPE || opcode == I_J_TYPE){
			alu_in.in2 = (imem_out.dout >> 20) & 0xFFF;
			//Input is a negative number
			if(alu_in.in2 & 0x800)
				alu_in.in2 = alu_in.in2 | 0xFFFFF000;
		}
		else if (opcode == U_LU_TYPE || opcode == U_AU_TYPE)
			imm = imem_out.dout & ~0xFFF;
		else if (opcode == UJ_TYPE){
			imm = 0;

			imm = imm | ((imem_out.dout >> 31) & 0x1) << 20;
			imm = imm | ((imem_out.dout >> 21) & 0x3FF) << 1;
			imm = imm | ((imem_out.dout >> 20) & 0x1) << 11;
			imm = imm | (imem_out.dout & 0xFF000);

			//Input is a negative number
			if(imm & 0x100000)
				imm = imm | 0xFFE00000;
		}
		else if (opcode == SB_TYPE){
			imm = 0;

			imm = imm | ((imem_out.dout >> 31) & 0x1) << 12;
			imm = imm | ((imem_out.dout >> 25) & 0x3F) << 5;
			imm = imm | ((imem_out.dout >> 8) & 0xF) << 1;
			imm = imm | ((imem_out.dout >> 7) & 0x1) << 11;
			//Input is a negative number
			if(imm & 0x1000)
				imm = imm | 0xFFFFE000;

			alu_in.in2 = regfile_out.rs2_dout;
		}
		else if (opcode == S_TYPE){
			imm = 0;

			imm = imm | ((imem_out.dout >> 7) & 0x1F);
			imm = imm | ((imem_out.dout >> 25) & 0x7F) << 5;
			//Input is a negative number
			if(imm & 0x800)
				imm = imm | 0xFFFFF000;
			alu_in.in2 = imm;
		}
		else
			alu_in.in2 = regfile_out.rs2_dout;


		// instruction fetch stage
		pc_curr = pc_next;
		imem_in.addr = pc_curr;

		imem_out = imem(imem_in, imem_data);

		// no more instruction
		if(!imem_out.dout)
			break;
		D_PRINTF("IF", "addr - 0x%08X", imem_in.addr);
		D_PRINTF("IF", "dout - 0x%08X", imem_out.dout);

		//for(int i = 0; i < REG_WIDTH; i++){
		for(int i = 0; i < 20; i++){
			printf("reg[%02d]: %08X\n", i, reg_data[i]);
		}
		printf("\n");
		for(int i = 0; i < 40; i += 4){
			printf("dmem[%02d]: ", i);
			for(int j = 3; j >= 0; j--)
				printf("%02X", dmem_data[i+j]);
			printf("\n");
		}
		
		cc++;
	}

	free(reg_data);
	free(imem_data);
	free(dmem_data);

	return 1;
}

struct imem_output_t imem(struct imem_input_t imem_in, uint32_t *imem_data) {
	
	struct imem_output_t imem_out;

	imem_out.dout = imem_data[imem_in.addr/4];

	return imem_out;
}

struct regfile_output_t regfile(struct regfile_input_t regfile_in, uint32_t *reg_data, enum REG regwrite){

	struct regfile_output_t regfile_out;

	if(regwrite == READ){
		regfile_out.rs1_dout = reg_data[regfile_in.rs1];
		if(regfile_in.rs2 < REG_WIDTH)
			regfile_out.rs2_dout = reg_data[regfile_in.rs2];
	}
	else if(regwrite  = WRITE){
		reg_data[regfile_in.rd] = regfile_in.rd_din;
	}

	return regfile_out;
}

struct alu_output_t alu(struct alu_input_t alu_in){

	struct alu_output_t alu_out;

	alu_out.zero = 1;
	alu_out.sign = 1;
	alu_out.ucmp = 0;

	switch(alu_in.alu_control){
		case C_AND:
			alu_out.result = alu_in.in1 & alu_in.in2;
			break;
		case C_OR:
			alu_out.result = alu_in.in1 | alu_in.in2;
			break;
		case C_XOR:
			alu_out.result = alu_in.in1 ^ alu_in.in2;
			break;
		case C_SL:
			alu_out.result = alu_in.in1 << (int32_t)alu_in.in2;
			break;
		case C_SR:
			alu_out.result = alu_in.in1 >> (int32_t)alu_in.in2;
			break;
		case C_SRA:
			alu_out.result = (int32_t)alu_in.in1 >> (int32_t)alu_in.in2;
			break;
		case C_SUB:
			alu_out.result = (int32_t)alu_in.in1 - (int32_t)alu_in.in2;
			break;
		default:
			alu_out.result = (int32_t)alu_in.in1 + (int32_t)alu_in.in2;
			break;
	}

	//does not handle the case of sign = 1 when result is zero
	if(alu_out.result)
		alu_out.zero = 0;
	if((int32_t)alu_out.result > 0)
		alu_out.sign = 0;
	if(alu_in.in1 < alu_in.in2)
		alu_out.ucmp = 1; 

	return alu_out;
}

uint8_t alu_control_gen(uint8_t opcode, uint8_t func3, uint8_t func7){

	if(opcode == I_L_TYPE || opcode == S_TYPE)
		return C_ADD;
	else if(opcode == SB_TYPE)
		return C_SUB;
	else {
		switch(func3){
		//Arithmetic & Shifts
			case F3_SL:
				return C_SL;
			case F3_ADD_SUB:
				if(((func7 >> 5) & 1))
					return C_SUB;
				else
					return C_ADD;
			case F3_SR:
				if(((func7 >> 5) & 1))
					return C_SRA;
				else
					return C_SR;
			case F3_XOR:
				return C_XOR;
			case F3_OR:
				return C_OR;
			case F3_AND:
				return C_AND;
			case F3_SLT:
			case F3_SLTU:
				return C_SUB;
		}
	}
}

struct dmem_output_t dmem(struct dmem_input_t dmem_in, uint8_t *dmem_data) {
	struct dmem_output_t dmem_out;

	if(dmem_in.mem_read){
		switch(dmem_in.func3){
			case LB:
			case LBU:
				dmem_out.dout = dmem_data[dmem_in.addr];
				//Negative number
				if(dmem_in.func3 == LB && dmem_out.dout & 0x80)
					dmem_out.dout |= 0xFFFFFF00;
				break;
			case LH:
			case LHU:
				dmem_out.dout = dmem_data[dmem_in.addr];
				dmem_out.dout |= dmem_data[dmem_in.addr+1] << BYTE_BIT;
				//Negative number
				if(dmem_in.func3 == LH && dmem_out.dout & 0x8000)
					dmem_out.dout |= 0xFFFF0000;
				break;
			case LW:
				dmem_out.dout = dmem_data[dmem_in.addr];
				dmem_out.dout |= dmem_data[dmem_in.addr+1] << BYTE_BIT;
				dmem_out.dout |= dmem_data[dmem_in.addr+2] << BYTE_BIT*2;
				dmem_out.dout |= dmem_data[dmem_in.addr+3] << BYTE_BIT*3;
				break;
		}
				D_PRINTF("MEM", "Read : %x", dmem_out.dout);
	}
	if(dmem_in.mem_write){
		switch(dmem_in.func3){
			case SB:
				dmem_data[dmem_in.addr] = (uint8_t)dmem_in.din;
				break;
			case SH:
				dmem_data[dmem_in.addr] = (uint8_t)dmem_in.din;
				dmem_data[dmem_in.addr+1] = (uint8_t)(dmem_in.din >> BYTE_BIT);
				break;
			case SW:
				dmem_data[dmem_in.addr] = (uint8_t)dmem_in.din;
				dmem_data[dmem_in.addr+1] = (uint8_t)(dmem_in.din >> BYTE_BIT);
				dmem_data[dmem_in.addr+2] = (uint8_t)(dmem_in.din >> BYTE_BIT*2);
				dmem_data[dmem_in.addr+3] = (uint8_t)(dmem_in.din >> BYTE_BIT*3);
				break;
		}
				D_PRINTF("MEM", "Write : %08X", *(uint32_t*)(&dmem_data[dmem_in.addr]));
	}

	return dmem_out;
}
