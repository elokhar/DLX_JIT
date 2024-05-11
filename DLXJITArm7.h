/* 
 * File:   DLXJITArm7.h
 * Author: TB
 *
 * Created on 16 listopada 2017, 11:02
 */
#pragma once
#if defined(__arm__)
#include "DLXJIT.h"
#include <vector>

enum Register
{
    R0 = 0x0,
    R1 = 0x1,
    R2 = 0x2,
    R3 = 0x3,
    R4 = 0x4,
    R5 = 0x5,
    R6 = 0x6,
    R7 = 0x7,
    R8 = 0x8,
    R9 = 0x9,
    R10 = 0xa,
    R11 = 0xb,
    R12 = 0xc,
    SP = 0xd,
    LR = 0xe,
    PC = 0xf
};

enum Condition
{
    EQ = 0x0,
    NE = 0x1,
    CS = 0x2,
    CC = 0x3,
    MI = 0x4,
    PL = 0x5,
    VS = 0x6,
    VC = 0x7,
    HI = 0x8,
    LS = 0x9,
    GE = 0xa,
    LT = 0xb,
    GT = 0xc,
    LE = 0xd,
    AL = 0xe,
    AL_special = 0xf
};

enum LoadStoreMode
{
    OFFSET,
    PREINDEXED,
    POSTINDEXED
};

struct DataProcessingRegisterInstruction
{
    uint8_t details2 : 4;
    bool b4 : 1;
    uint8_t op2 : 2;
    uint8_t imm5 : 5;

    uint8_t details1 : 8;
    uint8_t op : 5;
    bool b25 : 1;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));

struct DataProcessingImmediateInstruction
{
    uint16_t details : 16;
    uint8_t Rn : 4;
    uint8_t op : 5;
    bool b25 : 1;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));

struct MultiplyAndMupltyplyAndAccumulateInstruction
{
    uint8_t detail2 : 4;
    bool b4 : 1;
    bool b5 : 1; 
    bool b6 : 1; 
    bool b7 : 1; 
    uint16_t detail1: 12;
    uint8_t op : 4;
    bool b24 : 1;
    bool b25 : 1; 
    bool b26 : 1; 
    bool b27 : 1; 
    Condition cond : 4;
}__attribute__((__packed__));

struct LoadStoreInstruction
{
    uint16_t details : 16;
    Register Rn : 4; 
    uint8_t op1 : 5;
    bool A : 1;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));

struct BranchInstruction
{
    uint16_t details : 16;
    Register Rn : 4; 
    uint8_t op : 6;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));

struct BInstruction
{
    int32_t imm : 24;
    bool b24 : 1; 
    bool b25 : 1; 
    bool b26 : 1; 
    bool b27 : 1; 
    Condition cond : 4;
}__attribute__((__packed__));

struct MSRAndHintInstruction
{
    uint8_t op2 : 8;
    uint8_t details : 8;
    uint8_t op1 : 4; 
    bool b20 : 1; 
    bool b21 : 1;
    bool op : 1;
    bool b23 : 1;
    bool b24 : 1;
    bool b25 : 1;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));

struct MediaInstuction
{
    uint8_t detail2 : 4;
    bool b4 : 1;
    uint8_t op2 : 3;
    uint8_t detail1 : 8;
    uint8_t A : 4;
    uint8_t op1 : 3;
    bool b23 : 1;
    bool b24 : 1;
    bool b25 : 1;
    bool b26 : 1;
    bool b27 : 1;
    Condition cond : 4;
}__attribute__((__packed__));


class DLXJITArm7 : public DLXJIT {
public:
    typedef void(*DlxProgram)(uint8_t* data_memory);
    typedef std::vector<char> RawCodeContainer;
    
    DLXJITArm7();

    std::size_t getDataMemorySize() override;
    
    void execute() override;

    ~DLXJITArm7() override;
protected:
    uint8_t loadByteFromMemory(std::size_t address) override;
    void saveByteInMemory(std::size_t address, uint8_t data) override;
    uint16_t loadHalfFromMemory(std::size_t address) override;
    void saveHalfInMemory(std::size_t address, uint16_t data) override;
    uint32_t loadWordFromMemory(std::size_t address) override;
    void saveWordInMemory(std::size_t address, uint32_t data) override;
private:
    void writeNop(Condition cond);
    void writeMov(Condition cond, bool updateFlags,Register dest, Register src);
    void writeMov(Condition cond, bool updateFlags,Register dest, int16_t imm);
    
    void writeAdd(Condition cond, bool updateFlags, Register dest, Register src1, int16_t imm);
    void writeAdd(Condition cond, bool updateFlags, Register dest, Register src1, Register src2);
    void writeSub(Condition cond, bool updateFlags, Register dest, Register src1, int16_t imm);
    void writeSub(Condition cond, bool updateFlags, Register dest, Register src1, Register src2);
    void writeMul(Condition cond, bool updateFlags, Register dest, Register src1, Register src2);
    void writeMla(Condition cond, bool updateFlags, Register dest, Register src1, Register src2, Register src3);
    void writeRev(Condition cond, Register dst, Register src);
    
    void writeLDR(Condition cond, LoadStoreMode mode,  bool add, Register dst, Register base, uint16_t offset);
    void writeSTR(Condition cond, LoadStoreMode mode,  bool add, Register src, Register base, uint16_t offset);
    
    void writePush(Condition cond, Register src);
    void writePush(Condition cond, uint16_t registers);
    void writePop(Condition cond, Register dst);
    void writePop(Condition cond, uint16_t registers);
    void writeB(Condition cond, int32_t offset);
    
    Register loadDLXRegister(int no, int argumentNumber);
    
    Register getRegisterForTargetDLXRegister(int no);
    void storeTargetDLXRegister(int no);
    
    int32_t calcBranchOffset(CodCollection::size_type targetDlx, RawCodeContainer::size_type branchInstructionPosition);
    
    bool compileDLXInstruction(const DLXJITCodLine& line, const DLXJITCodLine& nextline);

    bool compileDLXInstruction(const DLXJITCodLine& line, const DLXJITCodLine& nextline, bool skip);

    void repairBranchOffsets();
    
    //void compileDLXInstruction(const DLXJITCodLine& line);

    void compile();
    std::vector<uint8_t> data;
    RawCodeContainer rawCode;
    std::vector<RawCodeContainer::size_type> dlxOffsetsInRawCode;
    DlxProgram program;
    
    struct JumpOffsetToRepair
    {
            std::shared_ptr<DLXJTypeTextInstruction> dlxInstruction;
            RawCodeContainer::size_type branchInstructionOffset;
    };

    std::vector<JumpOffsetToRepair> jumpOffsetsToRepair;
};
#endif