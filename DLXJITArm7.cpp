/* 
 * File:   DLXJITArm7.cpp
 * Author: TB
 * 
 * Created on 16 listopada 2017, 11:02
 */

#if defined(__arm__)
#include "DLXJITArm7.h"
#include <endian.h>
#include <cstring>
#include <sys/mman.h>
#include <initializer_list>
#include <iostream>
#include "utils.h"

using namespace std;

#define DATA_POINTER_REGISTER R0
#define FIRST_ARGUMENT_CACHE_REGISTER R8
#define SECOND_ARGUMENT_CACHE_REGISTER R9
#define RESULT_CACHE_REGISTER R10

template<typename T>
inline void serialize(std::vector<char>& code, const T& serializable)
{
	const char* ptr = (const char*)&serializable;
	for (int i = 0; i < sizeof(T); i++)
		code.push_back(ptr[i]);
}

inline uint16_t registersList(initializer_list<Register> list)
{
    uint16_t ret = 0;
    for(auto reg : list)
    {
        ret |= 1 << reg;
    }
    
    return ret;
}

inline int getDLXRegisterNumber(const std::string& name)
{
	if (!isRegisterName(name))
		throw DLXJITException(("Invalid register name: " + name).c_str());

	return stoi(name.substr(1));
}

inline int getDLXRegisterOffsetOnStack(int regNumber)
{
	return (regNumber - 8) * 4;
}

inline void setFlagsForLoadStoreMode(LoadStoreMode mode, bool &P, bool &W)
{
    switch(mode)
    {
        case OFFSET:
            P = true;
            W = false;
            break;
        case PREINDEXED:
            P = true;
            W = true;
            break;
        case POSTINDEXED:
            P = false;
            W = true;
            break;
    }
}


DLXJITArm7::DLXJITArm7() 
    : program(nullptr)
{
}

uint8_t DLXJITArm7::loadByteFromMemory(std::size_t address) {
    if (address + 1 > data.size())
            data.resize(address + 1);
    return *((uint8_t*)&data[address]);
}

void DLXJITArm7::saveByteInMemory(std::size_t address, uint8_t value) {
    if (address + 1 > data.size())
		data.resize(address + 1);
    *((uint8_t*)&data[address]) = value;
}

uint16_t DLXJITArm7::loadHalfFromMemory(std::size_t address) {
    if (address + 2 > data.size())
            data.resize(address + 2);
    return be16toh(*((uint16_t*)&data[address]));
}

void DLXJITArm7::saveHalfInMemory(std::size_t address, uint16_t value) {
    if (address + 2 > data.size())
		data.resize(address + 2);
    *((uint16_t*)&data[address]) = htobe16(value);
}

uint32_t DLXJITArm7::loadWordFromMemory(std::size_t address) {
    if (address + 4 > data.size())
            data.resize(address + 4);
    return be32toh(*((uint32_t*)&data[address]));
}

void DLXJITArm7::saveWordInMemory(std::size_t address, uint32_t value) {
    if (address + 4 > data.size())
		data.resize(address + 4);
    *((uint32_t*)&data[address]) = htobe32(value);
}

std::size_t DLXJITArm7::getDataMemorySize() {
    return data.size();
}

void DLXJITArm7::writeNop(Condition cond)
{
    MSRAndHintInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = true;
    instr.b24 = true;
    instr.b23 = false;
    instr.op = false;
    instr.b21 = true;
    instr.b20 = false;
    instr.op1 = 0x0;
    instr.details = 0xF0;
    instr.op2 = 0;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writeMov(Condition cond, bool updateFlags, Register dest, Register src) {
    DataProcessingRegisterInstruction instr;
    memset(&instr,0,sizeof(instr));
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = false;
    instr.op = 0xd << 1 | (updateFlags? 1 : 0);
    instr.details1 = dest;
    instr.imm5 = 0;
    instr.op2 = 0;
    instr.b4 = false;
    instr.details2 = src;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writeMov(Condition cond, bool updateFlags, Register dest, int16_t imm)
{
    DataProcessingImmediateInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = true;

    if (imm <= 0xFFF)
    {
        instr.op = 0xd << 1 | (updateFlags ? 1 : 0);
        instr.Rn = 0;
    }
    else
    {
        if (updateFlags)
            cout << "Warning: update flags is ignored because of 16 bit imm" << endl;
        instr.op = 0x10;
        instr.Rn = (imm & 0xF000) >> 12;
    }
    instr.details = dest << 12 | (imm & 0xFFF);

    serialize(rawCode, instr);
}



void DLXJITArm7::writeAdd(Condition cond, bool updateFlags, Register dest, Register src1, int16_t imm) {
    DataProcessingImmediateInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = true;
    instr.op = 0x4 << 1 | (updateFlags? 1 : 0);
    instr.Rn = src1;
    instr.details = dest << 12 | (imm & 0xFFF);
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writeAdd(Condition cond, bool updateFlags, Register dest, Register src1, Register src2) {
    DataProcessingRegisterInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = false;
    instr.op = 0x4 << 1 | (updateFlags ? 1 : 0);
    instr.details1 = src1 << 4 | dest;
    instr.imm5 = 0x0;
    instr.op2 = 0x0;
    instr.b4 = false;
    instr.details2 = src2;

    serialize(rawCode, instr);
}

void DLXJITArm7::writeSub(Condition cond, bool updateFlags, Register dest, Register src1, int16_t imm) {
    DataProcessingImmediateInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = true;
    instr.op = 0x2 << 1 | (updateFlags? 1 : 0);
    instr.Rn = src1;
    instr.details = dest << 12 | (imm & 0xFFF);
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writeSub(Condition cond, bool updateFlags, Register dest, Register src1, Register src2) {
    DataProcessingRegisterInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = false;
    instr.op = 0x2 << 1 | (updateFlags ? 1 : 0);
    instr.details1 = src1 << 4 | dest;
    instr.imm5 = 0x0;
    instr.op2 = 0x0;
    instr.b4 = false;
    instr.details2 = src2;

    serialize(rawCode, instr);
}

void DLXJITArm7::writeMul(Condition cond, bool updateFlags, Register dest, Register src1, Register src2)
{
    MultiplyAndMupltyplyAndAccumulateInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = false;
    instr.b24 = false;
    instr.op = 0x0 | (updateFlags ? 1 : 0);
    instr.detail1 = dest << 8 | src2;
    instr.b7 = true;
    instr.b6 = false;
    instr.b5 = false;
    instr.b4 = true;
    instr.detail2 = src1;

    serialize(rawCode, instr);
}

void DLXJITArm7::writeMla(Condition cond, bool updateFlags, Register dest, Register src1, Register src2, Register src3) 
{
    MultiplyAndMupltyplyAndAccumulateInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = false;
    instr.b25 = false;
    instr.b24 = false;
    instr.op = 0x1 << 1 | (updateFlags ? 1 : 0);
    instr.detail1 = dest << 8 | src3 << 4 | src2;
    instr.b7 = true;
    instr.b6 = false;
    instr.b5 = false;
    instr.b4 = true;
    instr.detail2 = src1;

    serialize(rawCode, instr);
}

void DLXJITArm7::writeRev(Condition cond, Register dest, Register src)
{
    MediaInstuction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = true;
    instr.b25 = true;
    instr.b24 = false;
    instr.b23 = true;
    instr.op1 = 0x3;
    instr.A = 0xF; //whatever
    instr.detail1 = dest << 4 | (0xF);
    instr.op2 = 0x1;
    instr.b4 = true;
    instr.detail2 = src;
    serialize(rawCode, instr);
}

void DLXJITArm7::writeLDR(Condition cond, LoadStoreMode mode,  bool add, Register dst, Register base, uint16_t offset) {
    LoadStoreInstruction instr;
    bool U = add;
    bool P,W;
    
    setFlagsForLoadStoreMode(mode,P,W);
    
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = true;
    instr.A = false;
    instr.op1 = (P ? 0x10 : 0) |
                (U ? 0x8 : 0) |
                (W ? 0x2 : 0) |
                0x1;
    
    instr.Rn = base;
    instr.details = dst << 12 | (offset & 0xFFF);
    serialize(rawCode,instr);
}

void DLXJITArm7::writeSTR(Condition cond, LoadStoreMode mode, bool add, Register src, Register base, uint16_t offset) {
    LoadStoreInstruction instr;
    bool U = add;
    bool P, W;

    setFlagsForLoadStoreMode(mode, P, W);

    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = true;
    instr.A = false;
    instr.op1 = (P ? 0x10 : 0) |
        (U ? 0x8 : 0) |
        (W ? 0x2 : 0) |
        0x0;

    instr.Rn = base;
    instr.details = src << 12 | (offset & 0xFFF);
    serialize(rawCode, instr);
}


void DLXJITArm7::writePush(Condition cond, Register src) {
    BranchInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = true;
    instr.op = 0x12;
    instr.Rn = SP;
    instr.details = src << 12 | 0x4;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writePush(Condition cond, uint16_t registers) {
    BranchInstruction instr;
    instr.cond = cond;
    instr.b27 = true;
    instr.b26 = false;
    instr.op = 0x12;
    instr.Rn = SP;
    instr.details = registers;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writePop(Condition cond, Register dst) {
    BranchInstruction instr;
    instr.cond = cond;
    instr.b27 = false;
    instr.b26 = true;
    instr.op = 0x9;
    instr.Rn = SP;
    instr.details = dst << 12 | 0x4;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writePop(Condition cond, uint16_t registers) {
    BranchInstruction instr;
    instr.cond = cond;
    instr.b27 = true;
    instr.b26 = false;
    instr.op = 0xb;
    instr.Rn = SP;
    instr.details = registers;
    
    serialize(rawCode,instr);
}

void DLXJITArm7::writeB(Condition cond, int32_t offset) {
    BInstruction instr;
    instr.cond = cond;
    instr.b27 = true;
    instr.b26 = false;
    instr.b25 = true;
    instr.b24 = false;
    instr.imm = offset >> 2;
    
    serialize(rawCode,instr);
}


Register DLXJITArm7::loadDLXRegister(int no,int argumentNumber) {
    if(no > 0 && no < 8)
    {
        return (Register)no;
    }
    else
    {
        Register ret;
        if (argumentNumber == 2)
            ret = R10;
        else
            ret = argumentNumber == 0 ? FIRST_ARGUMENT_CACHE_REGISTER : SECOND_ARGUMENT_CACHE_REGISTER;
        if(no == 0)
        {
            writeMov(AL,false,ret,0);
        }
        else
        {
            writeLDR(AL,OFFSET,true,ret,SP,getDLXRegisterOffsetOnStack(no));
        }
        return ret;
    }
}

Register DLXJITArm7::getRegisterForTargetDLXRegister(int no) {
    if(no > 0 && no < 8)
    {
        return (Register)no;
    }
    
    return RESULT_CACHE_REGISTER;
}

void DLXJITArm7::storeTargetDLXRegister(int no) {
    if(no > 0 && no < 8)
    {
        //NOP
        return;
    }
    
    writeSTR(AL,OFFSET,true,RESULT_CACHE_REGISTER,SP,getDLXRegisterOffsetOnStack(no));
}

int32_t DLXJITArm7::calcBranchOffset(CodCollection::size_type targetDlx, RawCodeContainer::size_type branchInstructionPosition ) {
    return dlxOffsetsInRawCode[targetDlx] - (branchInstructionPosition+8);
}


bool DLXJITArm7::compileDLXInstruction(const DLXJITCodLine& line, const DLXJITCodLine& nextline, bool skip_this) { //zwraca true gdy należy pominąć kolejną instrukcję
    bool skip_next;
    skip_next = false;
    if (skip_this)
    {
        //do nothing
    }
    else if (line.textInstruction->opcode() == "ADD")
    {
        shared_ptr<DLXRTypeTextInstruction> instr = dynamic_pointer_cast<DLXRTypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->reg(2));
        if(destination > 0)
        {
            Register src1 = loadDLXRegister(getDLXRegisterNumber(instr->reg(0)),0);
            Register src2 = loadDLXRegister(getDLXRegisterNumber(instr->reg(1)),1);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeAdd(AL,false,dst,src1,src2);
            storeTargetDLXRegister(destination);
        }
    }
    else if (line.textInstruction->opcode() == "ADDI")
    {
        shared_ptr<DLXITypeTextInstruction> instr = dynamic_pointer_cast<DLXITypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->reg(1));
        if (destination > 0)
        {
            Register src1 = loadDLXRegister(getDLXRegisterNumber(instr->reg(0)), 0);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeAdd(AL,false,dst,src1,instr->immediate());
            storeTargetDLXRegister(destination);
        }
    }
    else if (line.textInstruction->opcode() == "SUBI")
    {
        shared_ptr<DLXITypeTextInstruction> instr = dynamic_pointer_cast<DLXITypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->reg(1));
        if (destination > 0)
        {
            Register src1 = loadDLXRegister(getDLXRegisterNumber(instr->reg(0)), 0);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeSub(AL, false, dst, src1, instr->immediate());
            storeTargetDLXRegister(destination);
        }
    }
    else if (line.textInstruction->opcode() == "MULADD")
    {
        shared_ptr<DLXRTypeTextInstruction> instr = dynamic_pointer_cast<DLXRTypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->reg(2));
        if (destination > 0)
        {
            Register mul_src1 = loadDLXRegister(getDLXRegisterNumber(instr->reg(0)), 0);
            Register mul_src2 = loadDLXRegister(getDLXRegisterNumber(instr->reg(1)), 1);
            //writeMul(AL, false, mul_src1, mul_src1, mul_src2);
            Register add_src = loadDLXRegister(getDLXRegisterNumber(instr->reg(2)), 2);
            Register dst = getRegisterForTargetDLXRegister(destination);
            //writeAdd(AL, false, dst, mul_src1, add_src);
            writeMla(AL, false, dst, mul_src1, mul_src2, add_src);
            storeTargetDLXRegister(destination);
        }
    }
    else if (line.textInstruction->opcode() == "LOOPCHECK")
    {
        shared_ptr<DLXITypeTextInstruction> instr = dynamic_pointer_cast<DLXITypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->reg(1));
        if (destination > 0) //Zapisywanie do rejsetru R0 jest niedozwolone
        {
            Register src = loadDLXRegister(getDLXRegisterNumber(instr->reg(0)), 0);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeMov(AL, false, R9, instr->immediate());
            writeSub(AL, false, dst, R9, src);
            storeTargetDLXRegister(destination);
        }
        //Instrukcja LOOPCHECK wykonuje odejmuje imm64 od rejestru source (R1), a następnie zapisuje wynik do R3

    }
    else if (line.textInstruction->opcode() == "LDW" && nextline.textInstruction->opcode() == "STW" &&
     dynamic_pointer_cast<DLXMTypeTextInstruction>(line.textInstruction)->dataRegister() == 
        dynamic_pointer_cast<DLXMTypeTextInstruction>(nextline.textInstruction)->dataRegister())
    {
        shared_ptr<DLXMTypeTextInstruction> ldw_instr = dynamic_pointer_cast<DLXMTypeTextInstruction>(line.textInstruction);
        shared_ptr<DLXMTypeTextInstruction> stw_instr = dynamic_pointer_cast<DLXMTypeTextInstruction>(nextline.textInstruction);
        int destination = getDLXRegisterNumber(ldw_instr->dataRegister());
        if (destination > 0)
        {
            Register ldw_indexRegister = loadDLXRegister(getDLXRegisterNumber(ldw_instr->indexRegister()), 0);
            Register dataMemoryOffsetRegister = FIRST_ARGUMENT_CACHE_REGISTER;
            writeAdd(AL, false, dataMemoryOffsetRegister, ldw_indexRegister, DATA_POINTER_REGISTER);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeLDR(AL, OFFSET, true, dst, dataMemoryOffsetRegister, ldw_instr->baseAddress());

            Register stw_indexRegister = loadDLXRegister(getDLXRegisterNumber(stw_instr->indexRegister()), 0);
            //Register dataMemoryOffsetRegister = FIRST_ARGUMENT_CACHE_REGISTER;
            writeAdd(AL, false, dataMemoryOffsetRegister, stw_indexRegister, DATA_POINTER_REGISTER);
            //writeLDR(AL, OFFSET, true, dst, dataMemoryOffsetRegister, instr->baseAddress());
            writeSTR(AL, OFFSET, true, dst, dataMemoryOffsetRegister, stw_instr->baseAddress());
            
            writeRev(AL, dst, dst);
            storeTargetDLXRegister(destination);
        }

        skip_next = true;
    }

    else if (line.textInstruction->opcode() == "LDW")
    {
        shared_ptr<DLXMTypeTextInstruction> instr = dynamic_pointer_cast<DLXMTypeTextInstruction>(line.textInstruction);
        int destination = getDLXRegisterNumber(instr->dataRegister());
        if(destination > 0)
        {
            Register indexRegister = loadDLXRegister(getDLXRegisterNumber(instr->indexRegister()),0);
            Register dataMemoryOffsetRegister = FIRST_ARGUMENT_CACHE_REGISTER;
            writeAdd(AL,false,dataMemoryOffsetRegister,indexRegister,DATA_POINTER_REGISTER);
            Register dst = getRegisterForTargetDLXRegister(destination);
            writeLDR(AL,OFFSET,true,dst,dataMemoryOffsetRegister,instr->baseAddress());
            writeRev(AL,dst,dst);
            storeTargetDLXRegister(destination);
        }
    }
    else if (line.textInstruction->opcode() == "STW")
    {
        shared_ptr<DLXMTypeTextInstruction> instr = dynamic_pointer_cast<DLXMTypeTextInstruction>(line.textInstruction);
        int source = getDLXRegisterNumber(instr->dataRegister());
        Register indexRegister = loadDLXRegister(getDLXRegisterNumber(instr->indexRegister()), 0);
        Register dataMemoryOffsetRegister = FIRST_ARGUMENT_CACHE_REGISTER;
        writeAdd(AL, false, dataMemoryOffsetRegister, indexRegister, DATA_POINTER_REGISTER);
        Register src = getRegisterForTargetDLXRegister(source);
        writeRev(AL, src, src);
        //writeLDR(AL, OFFSET, true, dst, dataMemoryOffsetRegister, instr->baseAddress());
        writeSTR(AL, OFFSET, true, src, dataMemoryOffsetRegister, instr->baseAddress());
        writeRev(AL, src, src);
        //storeTargetDLXRegister(source);
    }
    else if (line.textInstruction->opcode() == "BRLE")
    {
        shared_ptr<DLXJTypeTextInstruction> instr = dynamic_pointer_cast<DLXJTypeTextInstruction>(line.textInstruction);
        int branch = getDLXRegisterNumber(instr->branchRegister());
        Register reg = loadDLXRegister(branch,0);
        writeMov(AL,true,FIRST_ARGUMENT_CACHE_REGISTER,reg);
        int32_t offset = 0;
        
        const string& label = instr->label();
        
        auto targetDlx =  getPositionForLabel(label);
        if(dlxOffsetsInRawCode.size() > targetDlx)
            offset = calcBranchOffset(targetDlx,rawCode.size());
        else
            jumpOffsetsToRepair.push_back({instr, rawCode.size()});
            
        writeB(LE,offset); 
    }
    else if (line.textInstruction->opcode() == "BRGE")
    {
        shared_ptr<DLXJTypeTextInstruction> instr = dynamic_pointer_cast<DLXJTypeTextInstruction>(line.textInstruction);
        int branch = getDLXRegisterNumber(instr->branchRegister());
        Register reg = loadDLXRegister(branch, 0);
        writeMov(AL, true, FIRST_ARGUMENT_CACHE_REGISTER, reg);
        int32_t offset = 0;

        const string& label = instr->label();

        auto targetDlx = getPositionForLabel(label);
        if (dlxOffsetsInRawCode.size() > targetDlx)
            offset = calcBranchOffset(targetDlx, rawCode.size());
        else
            jumpOffsetsToRepair.push_back({ instr, rawCode.size() });

        writeB(GE, offset);
    }
    else if (line.textInstruction->opcode() == "NOP")
    {
            //NOP ;)
    }
    else
    {
        //Poniższe linie można zakomentować w celu uruchomiania programu bez wszystkich rozkazów
        string message = "Unsupported DLX opcode: " + line.textInstruction->opcode();
        throw DLXJITException(message.c_str());
    }
    return skip_next;
}

void DLXJITArm7::repairBranchOffsets() {
    for(auto& toRepair : this->jumpOffsetsToRepair)
    {
        auto targetDLX = getPositionForLabel(toRepair.dlxInstruction->label());
        BInstruction& binstr = *((BInstruction*)&rawCode[toRepair.branchInstructionOffset]);
        binstr.imm = calcBranchOffset(targetDLX, toRepair.branchInstructionOffset) >> 2;
    }
}



void DLXJITArm7::compile() {
    rawCode.clear();
    
    writePush(AL,registersList({R4,R5,R6,R7,R8,R9,R10,R11,LR}));
    writeSub(AL,false,SP,SP,(numberOfDLXRegisters - 8)*4);
    bool skip_next = false;
    for (int i = 0; i < codContent.size(); i++)
    {
            dlxOffsetsInRawCode.push_back(rawCode.size());
            skip_next = compileDLXInstruction(codContent[i], codContent[i+1], skip_next);
            
    }
    writeAdd(AL,false,SP,SP,(numberOfDLXRegisters - 8)*4);
    writePop(AL,registersList({R4,R5,R6,R7,R8,R9,R10,R11,PC}));
    repairBranchOffsets();
    void* mem = mmap(
                NULL,
                rawCode.size(),
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_ANONYMOUS | MAP_PRIVATE,
                0,
                0);
    memcpy(mem, rawCode.data(), rawCode.size());

    program = (DlxProgram)mem;
    
#if 1
    {
        cerr << "Compiled program size: " << rawCode.size() << " bytes" << "\r\n";
        for (int i = 0; i < codContent.size(); i++)
        {
                cerr << (codContent[i].label == "" ? "" : codContent[i].label+":") << "\t" << codContent[i].textInstruction->toString() << ":\t0x" << std::hex << (((unsigned long)mem) + dlxOffsetsInRawCode[i]) << "\r\n";
        }
        cerr << std::dec;
    }
#endif
}



void DLXJITArm7::execute() {
    if(program == nullptr)
        compile();
    auto data_ptr = data.data();
    program(data_ptr);
}



DLXJITArm7::~DLXJITArm7() {
    if(program != nullptr)
        munmap((void*)program, rawCode.size());
}

#endif