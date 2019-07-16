/******************************************************************************
  File: opcode.c
  Created: 2019-06-04
  Updated: 2019-07-16
  Author: Aaron Oman
  Notice: Creative Commons Attribution 4.0 International License (CC-BY 4.0)
 ******************************************************************************/
#include <limits.h> // UINT_MAX
#include <stdlib.h> // rand, malloc, free
#include <string.h> // memset
#include <stdio.h>

#include "system.h"

struct opcode;
typedef void (*opcode_fn)(struct opcode *c, struct system *);
typedef void *(*allocator)(size_t);
typedef void (*deallocator)(void *);

struct opcode_fn_map {
        char *name;
        opcode_fn address;
        char *description;
};

// All instructions are 2 bytes store most-significant byte first. (Big Endian)
struct opcode {
        unsigned short jumpToInstruction;
        int skipNextInstruction;
        unsigned short instruction;
        opcode_fn fn;
        struct opcode_fn_map debug_fn_map[35];
};

static allocator ALLOCATOR = malloc;
static deallocator DEALLOCATOR = free;

void OpcodeMemControl(allocator Alloc, deallocator Dealloc) {
        ALLOCATOR = Alloc;
        DEALLOCATOR = Dealloc;
}

unsigned short OpcodeInstruction(struct opcode *c) {
        return c->instruction;
}

// Returns non-zero if successfully written.
int OpcodeDescription(struct opcode *c, char *str, unsigned int maxLen) {
        if (str == NULL) {
                return 0;
        }

        for (int i=0; i<35; i++) {
                if (c->fn == c->debug_fn_map[i].address) {
                        struct opcode_fn_map data = c->debug_fn_map[i];
                        snprintf(str, maxLen, "%s: %s%c", data.name, data.description, '\0');
                }
        }

        return !0;
}

unsigned int HighByte(struct opcode *c) {
        return c->instruction >> 8;
}

unsigned int LowByte(struct opcode *c) {
        return c->instruction & 0x00FF;
}

// pos: LSB first. 0 is LSB, 1 is next most significant byte, etc.
unsigned int NibbleAt(struct opcode *c, unsigned int pos) {
        switch (pos) {
                case 0:
                        return (c->instruction & 0x000F);
                        break;
                case 1:
                        return (c->instruction & 0x00F0) >> 4;
                        break;
                case 2:
                        return (c->instruction & 0x0F00) >> 8;
                        break;
                case 3:
                        return (c->instruction & 0xF000) >> 12;
                        break;
        }

        return UINT_MAX;
}

// See full fn listing: https://en.wikipedia.org/wiki/CHIP-8#Opcode_table

// Call: Calls RCA 1802 program at address NNN. Not necessary for most ROMs.
void Fn0NNN(struct opcode *c, struct system *s) {
        // Not Implemented.
}

// Display: Clears the screen.
void Fn00E0(struct opcode *c, struct system *s) {
        SystemClearScreen(s);
}

// Flow control: Returns from a subroutine.
void Fn00EE(struct opcode *c, struct system *s) {
        SystemPopStack(s);
}

// Flow control: goto NNN;
void Fn1NNN(struct opcode *c, struct system *s) {
        unsigned int low_byte = LowByte(c);
        unsigned int nibble = NibbleAt(c, 2);
        unsigned int address = ((nibble << 8) | low_byte);
        c->jumpToInstruction = address;
}

// Flow control: Call subroutine at NNN;
void Fn2NNN(struct opcode *c, struct system *s) {
        unsigned int low_byte = LowByte(c);
        unsigned int nibble = NibbleAt(c, 2);
        unsigned int address = ((nibble << 8) | low_byte);

        SystemPushStack(s);
        c->jumpToInstruction = address;
}

// Condition: Skip next instruction if VX equals NN.
void Fn3XNN(struct opcode *c, struct system *s) {
        unsigned int nn = LowByte(c);
        unsigned int x = NibbleAt(c, 2);

        if (s->v[x] == nn) {
                c->skipNextInstruction = 1;
        }
}

// Condition: Skip next instruction if VX doesn't equal NN.
void Fn4XNN(struct opcode *c, struct system *s) {
        unsigned int nn = LowByte(c);
        unsigned int x = NibbleAt(c, 2);

        if (s->v[x] != nn) {
                c->skipNextInstruction = 1;
        }
}

// Condition: Skip next instruction if VX equals VY.
void Fn5XY0(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        if (s->v[x] == s->v[y]) {
                c->skipNextInstruction = 1;
        }
}

// Constant expression: Sets VX to NN.
void Fn6XNN(struct opcode *c, struct system *s) {
        unsigned int nn = LowByte(c);
        unsigned int x = NibbleAt(c, 2);

        s->v[x] = nn;
}

// Constant expression: Adds NN to VX (carry flag is not changed).
void Fn7XNN(struct opcode *c, struct system *s) {
        unsigned int nn = LowByte(c);
        unsigned int x = NibbleAt(c, 2);

        s->v[x] += nn;
}

// Assignment: Sets VX to the value of VY.
void Fn8XY0(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        s->v[x] = s->v[y];
}

// Bitwise operation: Sets VX to: VX | VY.
void Fn8XY1(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        s->v[x] = s->v[x] | s->v[y];
}

// Bitwise operation: Sets VX to: VX & VY.
void Fn8XY2(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        s->v[x] = s->v[x] & s->v[y];
}

// Bitwise operation: Sets VX to: VX ^ VY.
void Fn8XY3(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        s->v[x] = s->v[x] ^ s->v[y];
}

// Math: Adds VY to VX. VF is set to 1 when there's a carry and 0 otherwise.
void Fn8XY4(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        int val = s->v[x] + s->v[y];
        s->v[x] = (unsigned char)val;

        if ((int)(s->v[x]) != val) {
                s->v[15] = 1;
        }
}

// Math: VY is subtracted from VX. VF is set to 0 when there's a borrow and 1 otherwise.
void Fn8XY5(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        int val = s->v[x] - s->v[y];
        s->v[x] = (unsigned char)val;

        s->v[15] = 1;
        if ((int)(s->v[x]) != val) {
                s->v[15] = 0;
        }
}

// Bitwise operation: Stores the least significant bit of VX in VF and then shifts VX to the right by 1.
void Fn8XY6(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        // unsigned int y = NibbleAt(c, 1);

        unsigned char lsb = s->v[x] | 0x01;
        s->v[15] = lsb;
        s->v[x] = (s->v[x] >> 1);
}

// Math: Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
void Fn8XY7(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        int val = s->v[y] - s->v[x];
        s->v[x] = (unsigned char)val;

        s->v[15] = 1;
        if ((int)(s->v[x]) != val) {
                s->v[15] = 0;
        }
}

// Bitwise operation: Stores the most significant bit of VX in VF and then shifts VX to the left by 1.
void Fn8XYE(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        unsigned char msb = s->v[x] | 0x80;
        s->v[15] = msb;
        s->v[x] = (s->v[x] << 1);
}

// Condition: Skips the next instruction if VX doesn't equal VY.
void Fn9XY0(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned int y = NibbleAt(c, 1);

        if (s->v[x] != s->v[y]) {
                c->skipNextInstruction = 1;
        }
}

// Memory: Sets I to the address NNN.
void FnANNN(struct opcode *c, struct system *s) {
        unsigned int low_byte = LowByte(c);
        unsigned int nibble = NibbleAt(c, 2);
        unsigned int address = ((nibble << 8) | low_byte);

        s->i = address;
}

// Flow control: Jumps to the address NNN plus V0.
void FnBNNN(struct opcode *c, struct system *s) {
        unsigned int low_byte = LowByte(c);
        unsigned int nibble = NibbleAt(c, 2);
        unsigned int address = ((nibble << 8) | low_byte);

        s->i = (s->v[0] + address);
}

// Random: Sets VX to the result of a bitwise AND on a random number (0-255) and NN.
void FnCXNN(struct opcode *c, struct system *s) {
        unsigned int nn = LowByte(c);
        unsigned int x = NibbleAt(c, 2);

        int r = (unsigned int)(rand() % 255);
        s->v[x] = nn & r;
}

// Display: Draws a sprite at coordinate (VX, VY) that has a width of 8 pixels
// and a height of N pixels. Each row of 8 pixels is read as bit-coded starting
// from memory location I; I value doesn’t change after the execution of this
// instruction. As described above, VF is set to 1 if any screen pixels are
// flipped from set to unset when the sprite is drawn, and to 0 if that doesn’t
// happen.
// I'm assuming (VX, VY) is the lower-left corner of the sprite, not the center.
void FnDXYN(struct opcode *c, struct system *s) {
        unsigned int x = s->v[NibbleAt(c, 2)];
        unsigned int y = s->v[NibbleAt(c, 1)];
        unsigned int height = NibbleAt(c, 0);

        SystemDrawSprite(s, x, y, height);
}

// Key operation: Skips the next instruction if the key stored in VX is pressed.
void FnEX9E(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned char key = s->v[x];

        if (s->key[key]) {
                c->skipNextInstruction = 1;
        }
}

// Key operation: Skips the next instruction if the key stored in VX isn't pressed.
void FnEXA1(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned char key = s->v[x];

        if (!(s->key[key])) {
                c->skipNextInstruction = 1;
        }
}

// Timer: Sets VX to the value of the delay timer.
void FnFX07(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        s->v[x] = SystemDelayTimer(s);
}

// Key operation: Block until a key press occurs, then store it in VX.
void FnFX0A(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        SystemWFKSet(s, x);
}

// Timer: Sets the delay timer to VX.
void FnFX15(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        SystemSetTimers(s, s->v[x], -1);
}

// Sound: Sets the sound timer to VX.
void FnFX18(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        SystemSetTimers(s, -1, s->v[x]);
        SystemSoundSetTrigger(s, 1);
}

// Memory: Adds VX to I.
void FnFX1E(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        s->i += s->v[x];
}

// Memory: Sets I to the location of the sprite for the character in
// VX. Characters 0-F (hex) are represented by a 4x5 font.
void FnFX29(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        unsigned char sprite = s->v[x];

        s->i = SystemFontSprite(s, sprite);
}

// Binary coded decimal: Stores the binary-coded decimal representation of VX,
// with the most significant of three digits at the address in I, the middle
// digit at I plus 1, and the least significant digit at I plus 2. (In other
// words, take the decimal representation of VX, place the hundreds digit in
// memory at location in I, the tens digit at location I+1, and the ones digit
// at location I+2.)
void FnFX33(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);
        unsigned char val = s->v[x];

        // Calculate starting at the ones digit, then tens, then hundreds.
        // Store most-significant-byte first, so:
        // s->memory[s->i+0] = hundreds digit
        // s->memory[s->i+1] = tens digit
        // s->memory[s->i+2] = ones digit
        //
        for (int i=0, j=3; i<3; i++, j--) {
                s->memory[s->i + j] = val % 10;
                val = val / 10;
        }
}

// Memory: Stores V0 to VX (inclusive) in memory starting at address I. I is
// unmodified.
void FnFX55(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        for (int i=0; i <= x; i++) {
                s->memory[s->i + i] = s->v[i];
        }
}

// Memory: Fills V0 to VX (inclusive) with values from memory starting at
// address I.  I is unmodified.
void FnFX65(struct opcode *c, struct system *s) {
        unsigned int x = NibbleAt(c, 2);

        for (int i=0; i <= x; i++) {
                s->v[i] = s->memory[s->i + i];
        }
}

struct opcode *OpcodeInit() {
        struct opcode *c = (struct opcode *)ALLOCATOR(sizeof(struct opcode));
        memset(c, 0, sizeof(struct opcode));

        c->instruction = 0;
        c->fn = NULL;
        c->skipNextInstruction = 0;
        c->jumpToInstruction = 0; // Can be zero for "off" because normal memory starts at 0x200

        c->debug_fn_map[0] = (struct opcode_fn_map){ "0NNN", Fn0NNN, "Call RCA 1802 program at address NNN. (NOP)" };
        c->debug_fn_map[1] = (struct opcode_fn_map){ "00E0", Fn00E0, "Clear the screen" };
        c->debug_fn_map[2] = (struct opcode_fn_map){ "00EE", Fn00EE, "Return from subroutine" };
        c->debug_fn_map[3] = (struct opcode_fn_map){ "1NNN", Fn1NNN, "Goto NNN" };
        c->debug_fn_map[4] = (struct opcode_fn_map){ "2NNN", Fn2NNN, "Call subroutine at NNN" };
        c->debug_fn_map[5] = (struct opcode_fn_map){ "3XNN", Fn3XNN, "Skip next instruction if VX equals NN" };
        c->debug_fn_map[6] = (struct opcode_fn_map){ "4XNN", Fn4XNN, "Skip next instruction if VX doesn't equal NN" };
        c->debug_fn_map[7] = (struct opcode_fn_map){ "5XY0", Fn5XY0, "Skip next instruction if VX equals VY" };
        c->debug_fn_map[8] = (struct opcode_fn_map){ "6XNN", Fn6XNN, "Set VX to NN" };
        c->debug_fn_map[9] = (struct opcode_fn_map){ "7XNN", Fn7XNN, "Add NN to VX without changing carry flag" };
        c->debug_fn_map[10] = (struct opcode_fn_map){ "8XY0", Fn8XY0, "Set VX to the value of VY" };
        c->debug_fn_map[11] = (struct opcode_fn_map){ "8XY1", Fn8XY1, "Set VX to VX | VY" };
        c->debug_fn_map[12] = (struct opcode_fn_map){ "8XY2", Fn8XY2, "Set VX to VX & VY" };
        c->debug_fn_map[13] = (struct opcode_fn_map){ "8XY3", Fn8XY3, "Set VX to VX ^ VY" };
        c->debug_fn_map[14] = (struct opcode_fn_map){ "8XY4", Fn8XY4, "Add VY to VX. FV is set to 1 on carry, otherwise 0" };
        c->debug_fn_map[15] = (struct opcode_fn_map){ "8XY5", Fn8XY5, "Subtract VY from VX. VF is set to 0 on a borrow, otherwise 1" };
        c->debug_fn_map[16] = (struct opcode_fn_map){ "8XY6", Fn8XY6, "Store the lsb of VX in VF then shift VX to the right by 1" };
        c->debug_fn_map[17] = (struct opcode_fn_map){ "8XY7", Fn8XY7, "Set VX to VY minus VX. VF is set to 0 on a borrow, otherwise 1" };
        c->debug_fn_map[18] = (struct opcode_fn_map){ "8XYE", Fn8XYE, "Store the msb of VX in VF then shift VX to the left by 1" };
        c->debug_fn_map[19] = (struct opcode_fn_map){ "9XY0", Fn9XY0, "Skip next instruction if VX doesn't equal VY" };
        c->debug_fn_map[20] = (struct opcode_fn_map){ "ANNN", FnANNN, "Set I to the addres NNN" };
        c->debug_fn_map[21] = (struct opcode_fn_map){ "BNNN", FnBNNN, "Jump to the address NNN plus V0" };
        c->debug_fn_map[22] = (struct opcode_fn_map){ "CXNN", FnCXNN, "Set VX to NN & R where R is a random number in [0-255]" };
        c->debug_fn_map[23] = (struct opcode_fn_map){ "DXYN", FnDXYN, "Draw sprite at (VX, VY)" };
        c->debug_fn_map[24] = (struct opcode_fn_map){ "EX9E", FnEX9E, "Skip next instruction if the key stored in VX is pressed" };
        c->debug_fn_map[25] = (struct opcode_fn_map){ "EXA1", FnEXA1, "Skip next instruction if key stored in VX isn't pressed" };
        c->debug_fn_map[26] = (struct opcode_fn_map){ "FX07", FnFX07, "Set VX to the value of the delay timer" };
        c->debug_fn_map[27] = (struct opcode_fn_map){ "FX0A", FnFX0A, "Block until a key press occurs, storing it in VX" };
        c->debug_fn_map[28] = (struct opcode_fn_map){ "FX15", FnFX15, "Set the delay timer to VX" };
        c->debug_fn_map[29] = (struct opcode_fn_map){ "FX18", FnFX18, "Set the sound timer to VX" };
        c->debug_fn_map[30] = (struct opcode_fn_map){ "FX1E", FnFX1E, "Add VX to I" };
        c->debug_fn_map[31] = (struct opcode_fn_map){ "FX29", FnFX29, "Set I to the location of the sprite for the character in VX" };
        c->debug_fn_map[32] = (struct opcode_fn_map){ "FX33", FnFX33, "Store big-endian binary-coded decimal representation of VX in memory starting at I" };
        c->debug_fn_map[33] = (struct opcode_fn_map){ "FX55", FnFX55, "Store V0 through VX in memory starting at I" };
        c->debug_fn_map[34] = (struct opcode_fn_map){ "FX65", FnFX65, "Fill V0 through VX with values from memory starting at I" };

        return c;
}

void OpcodeDeinit(struct opcode *c) {
        if (NULL == c)
                return;

        DEALLOCATOR(c);
}

void OpcodeDebug(struct opcode *c) {
        printf("struct opcode {\n");
        printf("\tinstruction: 0x%04X\n", c->instruction);
        printf("\tfn:_________ ");
        unsigned char *ptr = (unsigned char *)&c->fn;
        for (int i = 0; i < sizeof(opcode_fn); i++) {
                printf("%02x", ptr[i]);
        }
        printf("\n");

        for (int i=0; i<35; i++) {
                if (c->fn == c->debug_fn_map[i].address) {
                        printf("\tfn name:____ %s\n", c->debug_fn_map[i].name);
                        printf("\tfn desc:____ %s\n", c->debug_fn_map[i].description);
                }
        }
        printf("}\n");
}

// Stores two-byte opcode from memory pointed to by pc into opcode c.
void OpcodeFetch(struct opcode *c, struct system *s) {
        // NOTE: opcodes are stored as Big-Endian 16-bit values in memory.
        // We need to convert from Big-Endian to Little-Endian since I'm writing
        // this on x86-64.

        // Fetch the first byte from memory.
        // Left shift 8 bits, padding on the right with zeroes.
        // Binary OR with the next byte from memory.
        // eg.:
        //     A200
        //  OR 00B7
        //  =======
        //     A2B7
        c->instruction = s->memory[s->pc] << 8 | s->memory[s->pc + 1];
}

void OpcodeDecode(struct opcode *c, struct system *s) {
        unsigned int nibble = NibbleAt(c, 3);

        switch (nibble) {
                case 0: {
                        unsigned int low_byte = LowByte(c);
                        switch (low_byte) {
                                case 0xE0: {
                                        c->fn = Fn00E0;
                                } break;

                                case 0xEE: {
                                        c->fn = Fn00EE;
                                } break;

                                default: {
                                        c->fn = Fn0NNN;
                                } break;
                        }
                } break;

                case 1: {
                        c->fn = Fn1NNN;
                } break;

                case 2: {
                        c->fn = Fn2NNN;
                } break;

                case 3: {
                        c->fn = Fn3XNN;
                } break;

                case 4: {
                        c->fn = Fn4XNN;
                } break;

                case 5: {
                        c->fn = Fn5XY0;
                } break;

                case 6: {
                        c->fn = Fn6XNN;
                } break;

                case 7: {
                        c->fn = Fn7XNN;
                } break;

                case 8: {
                        unsigned int low_bit = NibbleAt(c, 0);
                        switch (low_bit) {
                                case 0: {
                                        c->fn = Fn8XY0;
                                } break;

                                case 1: {
                                        c->fn = Fn8XY1;
                                } break;

                                case 2: {
                                        c->fn = Fn8XY2;
                                } break;

                                case 3: {
                                        c->fn = Fn8XY3;
                                } break;

                                case 4: {
                                        c->fn = Fn8XY4;
                                } break;

                                case 5: {
                                        c->fn = Fn8XY5;
                                } break;

                                case 6: {
                                        c->fn = Fn8XY6;
                                } break;

                                case 7: {
                                        c->fn = Fn8XY7;
                                } break;

                                case 0xE: {
                                        c->fn = Fn8XYE;
                                } break;
                        }
                } break;

                case 9: {
                        c->fn = Fn9XY0;
                } break;

                case 0xA: {
                        c->fn = FnANNN;
                } break;

                case 0xB: {
                        c->fn = FnBNNN;
                } break;

                case 0xC: {
                        c->fn = FnCXNN;
                } break;

                case 0xD: {
                        c->fn = FnDXYN;
                } break;

                case 0xE: {
                        unsigned int low_byte = LowByte(c);
                        switch (low_byte) {
                                case 0x9E: {
                                        c->fn = FnEX9E;
                                } break;
                                case 0xA1: {
                                        c->fn = FnEXA1;
                                } break;
                        }
                } break;

                case 0xF: {
                        unsigned int low_byte = LowByte(c);
                        switch (low_byte) {
                                case 0x07: {
                                        c->fn = FnFX07;
                                } break;

                                case 0x0A: {
                                        c->fn = FnFX0A;
                                } break;

                                case 0x15: {
                                        c->fn = FnFX15;
                                } break;

                                case 0x18: {
                                        c->fn = FnFX18;
                                } break;

                                case 0x1E: {
                                        c->fn = FnFX1E;
                                } break;

                                case 0x29: {
                                        c->fn = FnFX29;
                                } break;

                                case 0x33: {
                                        c->fn = FnFX33;
                                } break;

                                case 0x55: {
                                        c->fn = FnFX55;
                                } break;

                                case 0x65: {
                                        c->fn = FnFX65;
                                } break;
                        }
                } break;
                default: {
                        printf("Unknown opcode: 0x%04X\n", c->instruction);
                        OpcodeDebug(c);
                }
        }
}

void OpcodeExecute(struct opcode *c, struct system *s) {
        if (c->fn == NULL) {
                printf("Unknown opcode instruction:\n");
                return;
        }

        c->fn(c, s);

        if (c->jumpToInstruction) {
                s->pc = c->jumpToInstruction;
                c->jumpToInstruction = 0;
        } else {
                SystemIncrementPC(s);
                if (c->skipNextInstruction) {
                        c->skipNextInstruction = 0;
                        SystemIncrementPC(s);
                }
        }
}
