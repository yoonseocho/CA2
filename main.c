#include <stdio.h>
#include <stdlib.h> // exit()
#include <string.h> // memset()

#define MEMORY_SIZE 0x4000000
#define REGISTER_SIZE 32
#define PC_END 0xFFFFFFFF

int inst_memory[MEMORY_SIZE] = {0, };
int memory[MEMORY_SIZE]= {0, };
int pc = 0;
int reg[REGISTER_SIZE] = {0, };

typedef struct {
    int opcode;
    int rs;
    int rt;
    int rd;

    int shamt;
    int func;
    int imm;
    int s_imm;
    int zero_ext_imm;
    int branch_addr;
    int jump_addr;
} Instruction;

typedef struct {
    int cycle_count;
    int r_type_count;
    int i_type_count;
    int j_type_count;
    int branch_count;
    int memory_access_count;
} ExecutionStats;

typedef struct {
    int RegDest;
    int RegDest_ra;
    int ALUSrc;
    int ALUOp;
    int MemtoReg;
    int MemRead;
    int RegWrite;
    int MemWrite;
    int Branch;
    int Jump;
    int JR;
    int JAL;
} ControlSignals;

typedef enum
{
    ADD, ADDI, ADDIU, ADDU, AND, ANDI, BEQ, BNE, J, JAL, JR, JALR, LUI, LW, NOR, OR, ORI, SLT, SLTI, SLTIU, SLTU, SLL, SRL, SW, SUB, SUBU, INVALID
} Opcode;

const char* OpcodeNames[] = {
    [ADD] = "add", [ADDI] = "addi", [ADDIU] = "addiu", [ADDU] = "addu",
    [AND] = "and", [ANDI] = "andi", [BEQ] = "beq", [BNE] = "bne",
    [J] = "j", [JAL] = "jal", [JR] = "jr", [JALR] = "jalr", [LUI] = "lui",
    [LW] = "lw", [NOR] = "nor", [OR] = "or", [ORI] = "ori",
    [SLT] = "slt", [SLTI] = "slti", [SLTIU] = "sltiu", [SLTU] = "sltu",
    [SLL] = "sll", [SRL] = "srl", [SW] = "sw", [SUB] = "sub", [SUBU] = "subu",
    [INVALID] = "invalid"
};

Opcode get_opcode(int opcode, int func);
void initiate();
int load_instructions(const char *filename);
int fetch();
Instruction decode(int inst_byte);
void print_instruction(Instruction decoded_inst);
void decode_and_update_stats(Instruction decoded_inst);
int ALU(int opcode, int rs, int rt,int s_imm, int shamt, int zero_ext_s_imm);
void branchAddr_ALU(int branch_Addr);
void pc_update_ALU();
void execute(Instruction decoded_inst, int* ALU_result, int* mem_index);
void accessMemory(Instruction decoded_inst, int mem_index, int* mem_value);
void writeBack(Instruction decoded_inst, int ALU_result, int mem_value);
void print_state(Instruction decoded_inst, int mem_index, int mem_value);
ExecutionStats stats = {0};
void print_result();
ControlSignals control;
void clearControlSignals(ControlSignals *control);
void set_control_signals(Opcode opcode);

int main(int argc, char*argv[]){
    if(argc < 2){
        fprintf(stderr, "Usage: %s <file_path>\n", argv[0]);
        return 1; // 일반적인 사용법 오류
    }

    int num_instructions = load_instructions(argv[1]);
    if(num_instructions < 0){
        fprintf(stderr, "Failed to load instructions from %s\n", argv[1]);
        return 1; // 파일 관련 오류
    }

    initiate(); 

    int ALU_result = 0;
    int mem_index = 0;
    int mem_value = 0;
    while(pc != PC_END){
        int inst_byte = fetch();
        Instruction decoded_inst = decode(inst_byte);
        // print_instruction(decoded_inst);
        decode_and_update_stats(decoded_inst);
        execute(decoded_inst, &ALU_result, &mem_index);
        accessMemory(decoded_inst, mem_index, &mem_value);
        writeBack(decoded_inst, ALU_result, mem_value);
        print_state(decoded_inst, mem_index, mem_value);
        stats.cycle_count++;
    }
    print_result();

    return 0;
}

Opcode get_opcode(int opcode, int func) {
    switch(opcode) {
        case 0x0:
            switch(func){
                case 0x20: return ADD;
                case 0x21: return ADDU;
                case 0x24: return AND;
                case 0x08: return JR;
                case 0x09: return JALR;
                case 0x27: return NOR;
                case 0x25: return OR;
                case 0x2A: return SLT;
                case 0x2B: return SLTU;
                case 0x00: return SLL;
                case 0x02: return SRL;
                case 0x22: return SUB;
                case 0x23: return SUBU;
            }
        case 0x8: return ADDI;
        case 0x9: return ADDIU;
        case 0xC: return ANDI;
        case 0x4: return BEQ;
        case 0x5: return BNE;
        case 0x2: return J;
        case 0x3: return JAL;
        case 0xF: return LUI;
        case 0x23: return LW;
        case 0xD: return ORI;
        case 0xA: return SLTI;
        case 0xB: return SLTIU;
        case 0x2B: return SW;
        default: return INVALID;
    } 
}

void initiate(){
    reg[29] = 0x1000000;
    reg[31] = 0xffffffff;
}

int load_instructions(const char *filename){
    FILE *fp = fopen(filename, "rb");
    if(!fp){
        perror("File opening failed");
        return -1; //파일 관련 오류
    }

    int inst_byte = 0;
    int count = 0;
    
    int i = 0;
    while(1){
        int numRead = fread(&inst_byte, sizeof(inst_byte),1,fp);
        if(numRead == 0){
            printf("All instructions are loaded to inst_mem\n\n");
            printf("===========single-cycle 시작=======================\n");
            break;
        }

        // printf("fread binary: 0x%x\n", inst_byte);

        //바이트 엔디언 변환(bin파일에서 읽은 데이터의 바이트 순서 변경) - 빅엔디언(0x123456678 -> 0x12가 가장 낮은주소에 위치), 리틀엔디언 to 빅엔디언
        unsigned int byte0, byte1, byte2, byte3;
        byte0 = inst_byte & 0x000000FF;
        byte1 = (inst_byte & 0x0000FF00) >> 8;
        byte2 = (inst_byte & 0x00FF0000) >> 16;
        byte3 = (inst_byte >> 24) & 0xFF;
        inst_byte = (byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3;

        // printf("reordered data: 0x%x\n", inst_byte);
        
        count++;
        inst_memory[i] = inst_byte;
        i++;
    }

    fclose(fp);

    return count;
}

int fetch(){ // pc가 가리키는 inst_mem address에서 inst를 가져옴
    return inst_memory[pc/4]; // inst는 항상 4 byte size
}

Instruction decode(int inst_byte){ // R,I,J타입 구분
    Instruction decoded_inst;
    decoded_inst.opcode = (inst_byte >> 26) & 0x3F;
    decoded_inst.rs = (inst_byte >> 21) & 0x1F;
    decoded_inst.rt = (inst_byte >> 16) & 0x1F;
    decoded_inst.rd = (inst_byte >> 11) & 0x1F;

    decoded_inst.shamt = (inst_byte >> 6) & 0x1F;
    decoded_inst.func = inst_byte & 0x3F;
    decoded_inst.imm = inst_byte & 0xFFFF;
    decoded_inst.s_imm =  (decoded_inst.imm & 0x8000) ?
                    (decoded_inst.imm | 0xFFFF0000) :
                    (decoded_inst.imm);
    decoded_inst.zero_ext_imm = decoded_inst.imm & 0xFFFF;
    decoded_inst.branch_addr = (decoded_inst.imm & 0x8000) ?
                            (0xFFFC0000 | (decoded_inst.imm << 2)) :
                            (decoded_inst.imm << 2);
    decoded_inst.jump_addr = (pc & 0xF0000000) | ((inst_byte & 0x3FFFFFF)<<2);
    return decoded_inst;
}

void print_instruction(Instruction inst){
    printf("opcode: 0x%x, rs: %d, rt: %d, rd: %d, s_imm: %d\n", 
        inst.opcode, inst.rs, inst.rt, inst.rd, inst.s_imm);
}

void decode_and_update_stats(Instruction decoded_inst){
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    if(decoded_inst.opcode == 0x0){
        stats.r_type_count++;
    } else if((opcode == J) || (opcode == JAL)){
        stats.j_type_count++;
    } else{
        stats.i_type_count++;
        if((opcode == BEQ) || (opcode == BNE)){
            stats.branch_count++;
        } else if((opcode == LW) || (opcode == SW)){
            stats.memory_access_count++;
        }
    }
}

int ALU(int opcode, int rs, int rt, int s_imm, int shamt, int zero_ext_imm){
    switch(opcode){
        case 0x0:
            switch(control.ALUOp){
                case 1:
                    if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                    fprintf(stderr, "Overflow error in ADD at pc=%x\n",pc);
                    exit(EXIT_FAILURE);
                    }
                case 2: return (unsigned)rs + (unsigned)rt;
                case 3: return rs & rt;
                case 4: return ~(rs | rt);
                case 5: return rs | rt;
                case 6: return (rs < rt) ? 1 : 0;
                case 7: return ((unsigned)rs < (unsigned)rt) ? 1 : 0;
                case 8: return rt << shamt;
                case 9: return rt >> shamt;
                case 10:
                    if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                    fprintf(stderr, "Overflow error in SUB at pc=%x\n",pc);
                    exit(EXIT_FAILURE);
                    }
                case 11: return (unsigned)rs - (unsigned)rt;
                default: return 0;
            }
        case 0x8:
            if((rs > 0 && rt > 0 && (rs+rt)<0) || (rs<0 && rt<0 && (rs+rt)>0)){
                fprintf(stderr, "Overflow error in ADDI at pc=%x\n",pc);
                exit(EXIT_FAILURE);
            }
        case 0x9: return (unsigned)rs + (unsigned)s_imm;
        case 0xC: return rs & zero_ext_imm;
        case 0xF: return s_imm << 16;
        case 0xD: return rs | zero_ext_imm;
        case 0xA: return (rs < s_imm) ? 1 : 0;
        case 0xB: return ((unsigned)rs < (unsigned)s_imm) ? 1 : 0;
        default: return 0;
    }
}

void branchAddr_ALU(int branch_Addr) {
    pc += branch_Addr;
}

void pc_update_ALU() {
    pc += 4;
}

void execute(Instruction decoded_inst, int* ALU_result, int* mem_index){ // 각 타입별 명령어 실행
    //명령어별로 구분하지 말고 input1, input2로 나눠서 구현..
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    set_control_signals(opcode);

    int input1 = reg[decoded_inst.rs];
    int input2 = control.ALUSrc ? decoded_inst.s_imm : reg[decoded_inst.rt]; // ALUSrc 신호에 따라 두 번째 ALU 입력 결정

    *ALU_result = ALU(decoded_inst.opcode, input1, input2, decoded_inst.s_imm, decoded_inst.shamt, decoded_inst.zero_ext_imm);

    if (control.MemRead || control.MemWrite) {
        int effective_address = input1 + decoded_inst.s_imm; // 유효 주소 계산
        *mem_index = effective_address / 4; // 메모리 인덱스 계산
        if (*mem_index < 0 || *mem_index >= MEMORY_SIZE) {
            fprintf(stderr, "Memory access error: Invalid memory index %d at PC=0x%x\n", *mem_index, pc);
            exit(EXIT_FAILURE);
        }
    }
    pc_update_ALU();
}

void accessMemory(Instruction decoded_inst, int mem_index, int* mem_value) {
    if (control.MemRead) {
        *mem_value = memory[mem_index];
    } else if (control.MemWrite) {
        memory[mem_index] = reg[decoded_inst.rt];
    }
}

void writeBack(Instruction decoded_inst, int ALU_result, int mem_value) {
    if (control.RegWrite) {
        int write_val = control.MemtoReg ? mem_value : ALU_result;
        if (control.RegDest) {
            reg[decoded_inst.rd] = write_val;
        } else if (control.RegDest_ra){
            reg[31] = pc + 4;
        } else{
            reg[decoded_inst.rt] = write_val;
        }
    }
    // writeBack에서 pc jump, beq update 처리하기
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    
    if (control.Branch && ((opcode == BEQ && reg[decoded_inst.rs] == reg[decoded_inst.rt]) || (opcode == BNE && reg[decoded_inst.rs] != reg[decoded_inst.rt]))) {
        branchAddr_ALU(decoded_inst.branch_addr);
    } else if (control.Jump || control.JAL) {
        pc = decoded_inst.jump_addr;
    } else if (control.JR) {
        pc = reg[decoded_inst.rs];
    }
}

void print_state(Instruction decoded_inst, int mem_index, int mem_value){
    // printf("R[%d] = %d, R[%d] = %d, R[%d] = %d, s_imm = %d\n",decoded_inst.rs, reg[decoded_inst.rs], decoded_inst.rt, reg[decoded_inst.rt], decoded_inst.rd, reg[decoded_inst.rd], decoded_inst.s_imm);
    Opcode opcode = get_opcode(decoded_inst.opcode, decoded_inst.func);
    switch(opcode) {
        case ADD:
        case ADDU:
        case AND:
        case NOR:
        case OR:
        case SLT:
        case SLTU:
        case SLL:
        case SRL:
        case SUB:
        case SUBU:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rd, reg[decoded_inst.rd]);
            break;
        case ADDI:
        case ADDIU:
        case ANDI:
        case LUI:
        case ORI:
        case SLTI:
        case SLTIU:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rt, reg[decoded_inst.rt]);
            break;
        case LW:
            printf("@0x%x : %s R[%d] : %d\n", pc-4, OpcodeNames[opcode], decoded_inst.rt, mem_value);
            break;
        case SW:
            printf("@0x%x : %s M[%d] : %d\n", pc-4, OpcodeNames[opcode], mem_index, mem_value);
            break;
        case BEQ:
        case BNE:
        case J:
        case JAL:
        case JR:
        case JALR:
            printf("%s PC : 0x%x\n",OpcodeNames[opcode], pc);
            break;
        default:
            break;
    }
}

void print_result(){
    printf("\n===========================PROGRAM RESULT=============================\n");
	printf("Return value R[2] : %d\n", reg[2]);
	printf("Total Cycle : %d\n", stats.cycle_count);
	printf("Executed 'R' instruction : %d\n", stats.r_type_count);
	printf("Executed 'I' instruction : %d\n", stats.i_type_count);
	printf("Executed 'J' instruction : %d\n", stats.j_type_count);
	printf("Number of Memory Access Instruction : %d\n", stats.memory_access_count);
    printf("Number of Branch Taken : %d\n", stats.branch_count);
    printf("======================================================================\n");
    return;
}

void clearControlSignals(ControlSignals *control) {
    memset(control, 0, sizeof(ControlSignals));
}

void set_control_signals(Opcode opcode){
    clearControlSignals(&control);

    switch(opcode) {
        case ADD:
            control.RegDest = 1;
            control.ALUOp = 1;
            control.RegWrite = 1;
            break;
        case ADDU:
            control.RegDest = 1;
            control.ALUOp = 2;
            control.RegWrite = 1;
            break;
        case AND:
            control.RegDest = 1;
            control.ALUOp = 3;
            control.RegWrite = 1;
            break;
        case NOR:
            control.RegDest = 1;
            control.ALUOp = 4;
            control.RegWrite = 1;
            break;
        case OR:
            control.RegDest = 1;
            control.ALUOp = 5;
            control.RegWrite = 1;
            break;
        case SLT:
            control.RegDest = 1;
            control.ALUOp = 6;
            control.RegWrite = 1;
            break;
        case SLTU:
            control.RegDest = 1;
            control.ALUOp = 7;
            control.RegWrite = 1;
            break;
        case SLL:
            control.RegDest = 1;
            control.ALUOp = 8;
            control.RegWrite = 1;
            break;
        case SRL:
            control.RegDest = 1;
            control.ALUOp = 9;
            control.RegWrite = 1;
            break;
        case SUB:
            control.RegDest = 1;
            control.ALUOp = 10;
            control.RegWrite = 1;
            break;
        case SUBU:
            control.RegDest = 1;
            control.ALUOp = 11;
            control.RegWrite = 1;
            break;
        case JR:
            control.ALUOp = 12;
            control.JR = 1;
            break;
        case JALR:
            control.ALUOp = 13;
            control.RegWrite = 1;
            control.RegDest_ra = 1;
            control.JR = 1;
            break;
        case ADDI:
        case ADDIU:
        case ANDI:
        case LUI:
        case ORI:
        case SLTI:
        case SLTIU:
            control.RegWrite = 1;
            control.ALUSrc = 1;
            break;
        case LW:
            control.RegWrite = 1;
            control.ALUSrc = 1;
            control.MemtoReg = 1;
            control.MemRead = 1;
            break;
        case SW:
            control.RegWrite = 0;
            control.ALUSrc = 1;
            control.MemWrite = 1;
            break;
        case BEQ:
        case BNE:
            control.ALUSrc = 1;
            control.Branch = 1;
            break;
        case J:
            control.Jump = 1;
            break;
        case JAL:
            control.RegWrite = 1;
            control.RegDest_ra = 1;
            control.JAL = 1;
            break;
    
        default:
            break;
    }
}