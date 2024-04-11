/* **************************************
 * Module: top design of rv32i pipelined processor
 *
 * Author: Sanghyeon Park
 *
 * **************************************
 */
#define DEBUG 1

#include "rv32i.h"

#define D_PRINTF(x, ...) \
	do {\
		if(DEBUG){ \
		printf("%s: ", x);\
		printf(__VA_ARGS__);\
		printf("\n");\
		}\
	}while(0)\


struct imem_output_t imem(struct imem_input_t imem_in, uint32_t *imem_data);
struct regfile_output_t regfile(struct regfile_input_t regfile_in, uint32_t *reg_data, enum REG regwrite);
struct alu_output_t alu(struct alu_input_t alu_in);
uint8_t alu_control_gen(uint8_t opcode, uint8_t func3, uint8_t func7);
struct dmem_output_t dmem(struct dmem_input_t dmem_in, uint8_t *dmem_data);


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
	uint8_t *dmem_data;

	reg_data = (uint32_t*)malloc(32*sizeof(uint32_t));
	imem_data = (uint32_t*)malloc(IMEM_DEPTH*sizeof(uint32_t));
	dmem_data = (uint8_t*)malloc(DMEM_DEPTH*sizeof(uint32_t));

	// initialize memory data
	int i, j, k;
	for (i = 0; i < 32; i++) reg_data[i] = 0;
	for (i = 0; i < IMEM_DEPTH; i++) imem_data[i] = 0;
	for (i = 0; i < DMEM_DEPTH*WORD_SIZE; i++) dmem_data[i] = 0;

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
 // For hex input
 //	while (fscanf(f_imem, "%8x", &buf) != EOF) {
 //		imem_data[i] = buf;
 //		printf("imem[%03d]: %08X\n", i, imem_data[i]);
 //		i++;
 //	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[2]);
	while (fscanf(f_dmem, "%8x", &buf) != EOF) {
		printf("dmem[%03d]: ", i);
	while (fscanf(f_imem, "%8x", &buf) != EOF) {
		imem_data[i] = buf;
		printf("imem[%03d]: %08X\n", i, imem_data[i]);
		i++;
	}
		for(int j = 0; j < WORD_SIZE; j++){
			dmem_data[i+j] = (buf & (0xFF << j*BYTE_BIT)) >> j*BYTE_BIT;
		}
		for(int j = WORD_SIZE-1; j >= 0; j--){
			printf("%02X", dmem_data[i+j]);
		}
		printf("\n");
		i += WORD_SIZE;
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

	// Pipeline registers
	struct pipe_if_id_t id;
	struct pipe_id_ex_t ex;
	struct pipe_ex_mem_t mem;
	struct pipe_mem_wb_t wb;

    // Hazard variable
    uint8_t id_flush;
    uint8_t id_stall;
    uint8_t if_flush;
    uint8_t if_stall;
    uint8_t pc_write;
    uint8_t branch_taken;
    
    // Result variable
    uint32_t hazard_count;

    //Clock count
	uint32_t cc = 0;	


    // Initialize variable
    pc_next = 0;
    id_flush = 0;
    id_stall = 0;
    if_flush = 0;
    if_stall = 0;
    pc_write = 1;
    branch_taken = 0;

    hazard_count = 0;

	while (cc < CLK_NUM) {
		printf("\n*** CLK : %d ***\n", cc);

		// Writeback stage
        if(wb.enable){
            D_PRINTF("WB", "PC - ************[%x]************", wb.pc_curr);
            if(!(wb.opcode == SB_TYPE || wb.opcode == S_TYPE)){
                // Get data from pipeline register
                pc_curr = wb.pc_curr;
                opcode = wb.opcode;
                imm = wb.imm;
                func3 = wb.func3;
                alu_out = wb.alu_out;
                dmem_out = wb.dmem_out;
                regfile_in = wb.regfile_in;

                // Main logic
                if(opcode == I_L_TYPE){
                    regfile_in.rd_din = dmem_out.dout;
                }
                
                D_PRINTF("WB", "[I]rd - %d", regfile_in.rd);
                D_PRINTF("WB", "[I]rd_din - 0x%X", regfile_in.rd_din);

                regfile_out = regfile(regfile_in, reg_data, WRITE);

                // Forwarding to EX stage (MEM hazard)
                // Check destination register num is not zero
                if(regfile_in.rd){
                    if(regfile_in.rd == ex.regfile_in.rs1){
                        ex.alu_in.in1 = regfile_in.rd_din;
                        D_PRINTF("WB", "rs1 forwarding %d", regfile_in.rd_din);
                    }
                    if(ex.opcode == R_TYPE && (regfile_in.rd == ex.regfile_in.rs2)){
                        ex.alu_in.in2 = regfile_in.rd_din;
                        D_PRINTF("WB", "rs2 forwarding %d", regfile_in.rd_din);
                    }
                }
            }
        }

		// Memory stage
        if(mem.enable){
            D_PRINTF("MEM", "PC - ************[%x]************", mem.pc_curr);
            // Get data from pipeline register
            pc_curr = mem.pc_curr;
            opcode = mem.opcode;
            imm = mem.imm;
            func3 = mem.func3;
            regfile_out = mem.regfile_out;
            alu_out = mem.alu_out;
            regfile_in = mem.regfile_in;
            
            // Main Logic
            if(opcode == S_TYPE || opcode == I_L_TYPE || opcode == I_J_TYPE){
                dmem_in.addr = alu_out.result;
                D_PRINTF("MEM", "[I]addr - 0x%X", dmem_in.addr);
                dmem_in.din = regfile_out.rs2_dout;
                D_PRINTF("MEM", "[I]din - 0x%X", dmem_in.din);
                dmem_in.func3 = func3;
                D_PRINTF("MEM", "[I]func3 - 0x%X", dmem_in.func3);

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

            // Forwarding to EX stage (EX hazard)
            // Check write to register
            if(!(opcode == SB_TYPE || opcode == S_TYPE)){
                if(regfile_in.rd){
                    if(regfile_in.rd == ex.regfile_in.rs1){
                        ex.alu_in.in1 = alu_out.result;
                        D_PRINTF("MEM", "rs1 forwarding %d", alu_out.result);
                    }
                    if(ex.opcode == R_TYPE || ex.opcode == SB_TYPE){
                        if(regfile_in.rd == ex.regfile_in.rs2){
                            if(opcode == I_L_TYPE){
                                ex.alu_in.in2 = dmem_out.dout;
                                D_PRINTF("MEM", "rs2 forwarding(I_L_TYPE) %d", dmem_out.dout);
                            }
                            else{
                                ex.alu_in.in2 = alu_out.result;
                                D_PRINTF("MEM", "rs2 forwarding %d", alu_out.result);
                            }
                        }
                    }
                    else if(ex.opcode == S_TYPE){
                        if(regfile_in.rd == ex.regfile_in.rs2){
                            ex.regfile_out.rs2_dout = alu_out.result;
                            D_PRINTF("MEM", "rs2 forwarding(S_TYPE) %d", alu_out.result);
                        }
                    }
                }
            }

            //Update pipeline register
            wb.enable = 1;
            wb.pc_curr = pc_curr;
            wb.opcode = opcode;
            wb.imm = imm;
            wb.func3 = func3;
            wb.alu_out = alu_out;
            wb.dmem_out = dmem_out;
            wb.regfile_in = regfile_in;

        }
        else{
            wb.enable = 0;
        }

		// Execute stage
        if(ex.enable && !id_flush && !id_stall){
            D_PRINTF("EX", "PC - ************[%x]************", ex.pc_curr);
                
            // Get data from pipeline register
            pc_curr = ex.pc_curr;
            opcode = ex.opcode;
            imm = ex.imm;
            func3 = ex.func3;
            func7 = ex.func7;
            alu_in = ex.alu_in;
            regfile_in = ex.regfile_in;


            // Main logic
            alu_in.alu_control = alu_control_gen(opcode, func3, func7);

            D_PRINTF("EX", "[I]in1 - %d", alu_in.in1);
            D_PRINTF("EX", "[I]in2 - %d", (int32_t) alu_in.in2);
            D_PRINTF("EX", "[I]alu_cont - %x", alu_in.alu_control);

            alu_out = alu(alu_in);

            int8_t pc_next_sel = 0;

            // Check the branch is taken
            if(opcode == SB_TYPE){
                switch(func3){
                    case F3_BEQ:
                        pc_next_sel = alu_out.zero ? 1 : 0;
                        break;
                    case F3_BNE:
                        pc_next_sel = alu_out.zero ? 0 : 1;
                        break;
                    case F3_BLT:
                        pc_next_sel = (!alu_out.zero && alu_out.sign) 
                            ? 1 : 0;
                        break;
                    case F3_BGE:
                        pc_next_sel = (alu_out.zero || !alu_out.sign) 
                            ? 1 : 0;
                        break;
                    case F3_BLTU:
                        pc_next_sel = (!alu_out.zero && alu_out.ucmp) 
                            ? 1 : 0;
                        break;
                    case F3_BGEU:
                        pc_next_sel = (alu_out.zero || !alu_out.ucmp) 
                            ? 1 : 0;
                        break;
                }
            }

            // When the branch is taken, Calculate taken address
            if(pc_next_sel){
                pc_next = pc_curr + (int32_t)imm;
                branch_taken = 1;
            }
            else if(opcode == UJ_TYPE){
                pc_next = pc_curr + (int32_t)imm;
                branch_taken = 1;
            }
            else if(opcode == I_J_TYPE){
                pc_next = alu_out.result;
                branch_taken = 1;
            }

            //Calculate register write value
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
            }

            D_PRINTF("EX", "branch_taken - %d", branch_taken);
            D_PRINTF("EX", "[I]rd_din - %d", regfile_in.rd_din);
            D_PRINTF("EX", "[I]result - %d", alu_out.result);
            D_PRINTF("EX", "[I]zero - %d", alu_out.zero);
            D_PRINTF("EX", "[I]sign - %d", alu_out.sign);

            // Update pipeline register
            mem.enable = 1;
            mem.pc_curr = pc_curr;
            mem.opcode = opcode;
            mem.imm = imm;
            mem.func3 = func3;
            mem.alu_out = alu_out; 
            mem.regfile_in = regfile_in;
            //Not use in this stage
            mem.regfile_out = ex.regfile_out;
        }
        else{
            mem.enable = 0;
        }

		// Instruction decode stage
        if(id.enable && !if_flush && !if_stall){
            D_PRINTF("ID", "PC - ************[%x]************", id.pc_curr);

            // Get data from pipeline register
            pc_curr = id.pc_curr;
            imem_out = id.imem_out;

            // Main logic
            opcode = imem_out.dout & 0x7F;
            D_PRINTF("ID", "[I]opcode- %x", opcode);

            if (!(opcode == U_LU_TYPE || opcode == U_AU_TYPE 
                        || opcode == UJ_TYPE))
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

            // Register Read
            regfile_out = regfile(regfile_in, reg_data, READ);


            D_PRINTF("ID", "[O]rs1_dout - %d", regfile_out.rs1_dout);
            if (opcode == SB_TYPE || opcode == R_TYPE || opcode == S_TYPE)
                D_PRINTF("ID", "[O]rs2_dout - %d", regfile_out.rs2_dout);
            
            alu_in.in1 = regfile_out.rs1_dout;

            //Immediate generation
            if (opcode == I_L_TYPE || opcode == I_R_TYPE || opcode == I_J_TYPE){
                imm = (imem_out.dout >> 20) & 0xFFF;
                //Input is a negative number
                if(imm & 0x800)
                    imm = imm | 0xFFFFF000;
                alu_in.in2 = imm;
            }
            else if (opcode == U_LU_TYPE || opcode == U_AU_TYPE)
                imm = imem_out.dout & ~0xFFF;
            else if (opcode == UJ_TYPE){
                imm = 0;
                imm = imm | ((imem_out.dout >> 31) & 0x1) << 20;
                imm = imm | ((imem_out.dout >> 21) & 0x3FF) << 1;
                imm = imm | ((imem_out.dout >> 20) & 0x1) << 11;
                imm = imm | (imem_out.dout & 0xFF000);

                // When the input is a negative number
                if(imm & 0x100000)
                    imm = imm | 0xFFE00000;
            }
            else if (opcode == SB_TYPE){
                imm = 0;
                imm = imm | ((imem_out.dout >> 31) & 0x1) << 12;
                imm = imm | ((imem_out.dout >> 25) & 0x3F) << 5;
                imm = imm | ((imem_out.dout >> 8) & 0xF) << 1;
                imm = imm | ((imem_out.dout >> 7) & 0x1) << 11;

                // When the input is a negative number
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

            D_PRINTF("ID", "[I]imm - %d", imm);

            //Hazard Detection Unit
            if(mem.opcode == I_L_TYPE){
                if(mem.regfile_in.rd == regfile_in.rs1 || 
                        mem.regfile_in.rd == regfile_in.rs2){
                    D_PRINTF("ID", "Hazard Detect: [%x]", mem.pc_curr);
                    //id_stall = 1;
                    if_stall = 1;
                    //pre_pc_write = 0;
                    pc_write = 0;
                    hazard_count++;
                }
            }

            // Update pipeline register
            ex.enable = 1;
            ex.pc_curr = pc_curr;
            ex.opcode = opcode;
            ex.imm = imm;
            ex.func3 = func3;
            ex.func7 = func7;
            ex.alu_in = alu_in; 
            ex.regfile_in = regfile_in;
            ex.regfile_out = regfile_out;
        }
        else{
            ex.enable = 0;
            if_stall = 0;
        }

		// Instruction fetch stage
        if(pc_write){
            D_PRINTF("IF", "PC - ****************************");
            pc_curr = pc_next;

            D_PRINTF("IF", "pc_curr : %X", pc_curr);
            imem_in.addr = pc_curr;

            imem_out = imem(imem_in, imem_data);
            D_PRINTF("IF", "imem_out.dout: 0x%08X", imem_out.dout);

            // Program counter

            // Handle the flush signal
            if(branch_taken){
                //Flush
                id_flush = 1;
                if_flush = 1;

                branch_taken = 0;

                D_PRINTF("PC", "Take branch");
            }
            else{
                //Not taken
                pc_next = pc_curr + 4;
                id_flush = 0;
                if_flush = 0;
            }

            D_PRINTF("PC", "pc_next : %X", pc_next);

            // Update pipeline register
            id.enable = 1;
            id.pc_curr = pc_curr;
            id.imem_out= imem_out;
        }
        else{
            pc_write = 1;
        }

        // Print state
		//for(int i = 0; i < REG_WIDTH; i++){
		for(int i = 0; i < 32; i++){
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

    // Result
    printf("Hazard count : %d\n", hazard_count);

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
	else if(regwrite == WRITE){
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
    //For Error case
    return 0b1111;
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
