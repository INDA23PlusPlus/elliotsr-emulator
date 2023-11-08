#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define THROW(err, msg, ...) (fprintf(stderr, msg "\n",##__VA_ARGS__), err)

typedef uint8_t byte_t;
typedef uint64_t width_t;
typedef uint16_t addr_t;
typedef uint16_t opcode_t;

static byte_t Memory[4096] = {0}; // General Memory
static width_t Display[32] = {0}; // Display Memory
static addr_t Stack[32] = {0}; // Stack Memory

static addr_t PC = 0; // Program Counter
static addr_t IDX = 0; // Index Register
static byte_t DelayTimer = 0; // Delay Timer Register
static byte_t SoundTimer = 0; // Sound Timer Register

static byte_t V[16] = {0}; // General Registers (V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF)

typedef struct {
    byte_t op;
    byte_t X;
    byte_t Y;
    byte_t N;
    byte_t NN;
    addr_t NNN;
} instr_t;

// reversed?
opcode_t fetch(void) {
    opcode_t opcode = 0x0000;
    opcode |= ((opcode_t)Memory[PC++]) << 8;
    opcode |= ((opcode_t)Memory[PC++]) << 0;
    return opcode;
}

instr_t decode(opcode_t opcode) {
    instr_t instr;
    instr.op = (opcode & 0xF000) >> 12;
    instr.X = (opcode & 0x0F00) >> 8;
    instr.Y = (opcode & 0x00F0) >> 4;
    instr.N = opcode & 0x000F;
    instr.NN = opcode & 0x00FF;
    instr.NNN = opcode & 0x0FFF;
    return instr;
}

// 00E0 (clear screen)
// 1NNN (jump)
// 6XNN (set register VX)
// 7XNN (add value to register VX)
// ANNN (set index register I)
// DXYN (display/draw)
void execute(instr_t instr) {

    byte_t op = instr.op;
    byte_t X = instr.X;
    byte_t Y = instr.Y;
    byte_t N = instr.N;
    byte_t NN = instr.NN;
    addr_t NNN = instr.NNN;

    switch (op) {

        // Clear Screen
        case 0x00: {
            for (byte_t i = 0; i < 32; i++) {
                Display[i] = 0;
            }
            break;
        }

        // Jump To NNN
        case 0x01: {
            PC = NNN;
            break;
        }

        // Set Register VX To NN
        case 0x06: {
            V[X] = NN;
            break;
        }

        // Add NN To Register VX
        case 0x07: {
            V[X] += NN;
            break;
        }

        // Set Index Register to NNN
        case 0x0A: {
            IDX = NNN;
            break;
        }

        // Draw To Display
        case 0x0D: {

            // byte_t x = Register[X] & 63;
            // byte_t y = Register[Y] & 31;
            // Register[REG_VF] = 0;
            // for (byte_t iy = 0; iy < N; iy++) {
            //     byte_t sprite = Memory[IDX + iy];
            //     for (byte_t ix = 0; ix < 8; ix++) {
            //         width_t dmask = (((width_t)1) << x);
            //         byte_t bmask = ((((byte_t)1) << 7) >> ix);
            //         byte_t sbit = sprite & bmask;
            //         width_t dbit = Display[y] & dmask;
            //         if (sbit != 0 && dbit != 0) {
            //             Display[y] &= ~dmask;
            //             Register[REG_VF] = 1;
            //         } else if (sbit != 0 && dbit == 0) {
            //             Display[y] |= dmask;
            //         }
            //         if (x > 63) break;
            //         x++;
            //     }
            //     if (y > 31) break;
            //     y++;
            // }

            // DXYN(V[X], V[Y], N);

            byte_t x = V[X] & 63;
            byte_t y = V[Y] & 31;

            V[0xF] = 0;
            for (byte_t iy = 0; iy < N; iy++) {
                byte_t px = Memory[IDX + iy];
                for (byte_t ix = 0; ix < 8; ix++) {
                    if((px & (0x80 >> ix)) != 0) {
                        width_t mask = ((width_t)1) << (x + ix);
                        if((Display[y + iy] & mask) != 0) {
                            V[0xF] = 1;
                        }
                        Display[y + iy] ^= (((width_t)1) << (x + ix));
                    }
                }
            }
            break;
        }


    }
}

size_t ch8load(FILE* fp, size_t size, addr_t addr) {
    return fread(&Memory[addr], size, 1, fp);
}

void ch8display(void) {
    printf("\x1b[H"); // ANSI HOME
    printf("\x1b[2J"); // ANSI CLEAR
    for (byte_t y = 0; y < 32; y++) {
        for (byte_t x = 0; x < 64; x++) {
            width_t mask = ((width_t)1) << x;
            if ((Display[y] & mask) != 0) {
                printf("\x1b[47m"); // ANSI WHITE BG
            } else {
                printf("\x1b[40m"); // ANSI BLACK BG
            }
            putchar(' ');
            putchar(' ');
            printf("\x1b[0m"); // ANSI RESET
        }
        putchar('\n');
    }
}

int main(void) {

    PC = 0x200;
    IDX = 0x0;

    FILE* font = fopen("../resource/font.ch8", "rb");
    if (font == NULL) return THROW(1, "Error opening font file");
    ch8load(font, 80, IDX);
    fclose(font);

    FILE* exe = fopen("../resource/IBM Logo.ch8", "rb");
    if (exe == NULL) return THROW(1, "Error opening executable file");
    ch8load(exe, 1024, PC);
    fclose(exe);

    clock_t t0, t1;
    float t, dt;

    float delay = 1.0f / 60.0f;

    t0 = clock();

    for (;;) {

        t1 = clock();
        dt = (t1 - t0) / (float)CLOCKS_PER_SEC;
        t0 = t1;

        t += dt;

        while (t > delay) {

            opcode_t opcode = fetch();
            instr_t instr = decode(opcode);
            execute(instr);

            ch8display();

            if (DelayTimer > 0)
                DelayTimer--;

            if (SoundTimer > 0)
                SoundTimer--;

            t -= delay;
        }

    }

    return 0;
}