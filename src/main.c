#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define ERR(err, msg, ...) (fprintf(stderr, msg "\n",##__VA_ARGS__), err)

typedef uint8_t byte_t;
typedef uint64_t width_t;
typedef uint16_t addr_t;
typedef uint16_t opcode_t;
typedef uint16_t keyboard_t;

static byte_t Memory[4096] = {0}; // General Memory
static width_t Display[32] = {0}; // Display Memory
static addr_t Stack[32] = {0}; // Stack Memory
static keyboard_t Keyboard = 0; // Keyboard State

static addr_t SP = 0; // Stack Pointer
static addr_t PC = 0; // Program Counter
static addr_t I = 0; // Index Register
static byte_t DelayTimer = 0; // Delay Timer
static byte_t SoundTimer = 0; // Sound Timer

static byte_t V[16] = {0}; // General Registers (V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF)

static byte_t Draw = 0; // Draw Flag

static byte_t getkey(byte_t i) {
    return (Keyboard & (((keyboard_t)1) << i)) != 0;
} 

typedef struct {
    byte_t op;
    byte_t X;
    byte_t Y;
    byte_t N;
    byte_t NN;
    addr_t NNN;
} instr_t;

opcode_t fetch(void) {
    opcode_t opcode = 0x0000;
    opcode |= ((opcode_t)Memory[PC]) << 8;
    opcode |= ((opcode_t)Memory[PC+1]) << 0;
    PC += 2;
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

void execute(instr_t instr) {

    byte_t op = instr.op;
    byte_t X = instr.X;
    byte_t Y = instr.Y;
    byte_t N = instr.N;
    byte_t NN = instr.NN;
    addr_t NNN = instr.NNN;

    switch (op) {

        case 0x00: {

            switch (NN) {

                case 0xE0: {
                    for (byte_t i = 0; i < 32; i++) {
                        Display[i] = 0;
                    }
                    break;
                }

                case 0xEE: {
                    PC = Stack[SP--];
                    break;
                }

            }

            break;
        }

        case 0x01: {
            PC = NNN;
            break;
        }

        case 0x02: {
            Stack[++SP] = PC;
            PC = NNN;
            break;
        }

        case 0x03: {
            if (V[X] == NN) {
                PC += 2;
            }
            break;
        }

        case 0x04: {
            if (V[X] != NN) {
                PC += 2;
            }
            break;
        }

        case 0x05: {
            if (V[X] == V[Y]) {
                PC += 2;
            }
            break;
        }

        case 0x06: {
            V[X] = NN;
            break;
        }

        case 0x07: {
            V[X] += NN;
            break;
        }

        case 0x08: {

            switch (N) {
                
                case 0x0: {
                    V[X] = V[Y];
                    break;
                }

                case 0x1: {
                    V[X] = V[X] | V[Y];
                    break;
                }

                case 0x2: {
                    V[X] = V[X] & V[Y];
                    break;
                }

                case 0x3: {
                    V[X] = V[X] ^ V[Y];
                    break;
                }

                // idk
                case 0x4: {
                    V[0xF] = __builtin_add_overflow(V[X], V[Y], &V[X]);
                    break;
                }

                case 0x5: {
                    V[0xF] = V[X] > V[Y];
                    V[X] = V[X] - V[Y];
                    break;
                }

                case 0x6: {
                    V[0xF] = (V[X] & 0b00000001) != 0;
                    V[X] >>= 1;
                    break;
                }

                case 0x7: {
                    V[0xF] = V[Y] > V[X];
                    V[X] = V[Y] - V[X];
                    break;
                }

                case 0xE: {
                    V[0xF] = (V[X] & 0b10000000) != 0;
                    V[X] <<= 1;
                    break;
                }

            }

            break;
        }

        case 0x09: {
            if (V[X] != V[Y]) {
                PC += 2;
            }
            break;
        }

        case 0x0A: {
            I = NNN;
            break;
        }

        case 0x0B: {
            PC = NNN + V[0x0];
            break;
        }

        case 0x0C: {
            byte_t rnd = clock() % 255;
            rnd ^= (rnd << 13);
            rnd ^= (rnd >> 17);
            rnd ^= (rnd << 5);
            V[X] = rnd & NN;
            break;
        }

        case 0x0D: {
            byte_t x = V[X] & 63;
            byte_t y = V[Y] & 31;

            V[0xF] = 0;
            for (byte_t r = 0; r < N; r++) {
                byte_t px = Memory[I + r];
                for (byte_t c = 0; c < 8; c++) {
                    if((px & (0x80 >> c)) != 0) {
                        width_t mask = ((width_t)1) << (x + c);
                        if((Display[y + r] & mask) != 0) {
                            V[0xF] = 1;
                        }
                        Display[y + r] ^= (((width_t)1) << (x + c));
                    }
                }
            }

            Draw = 1;
            break;
        }

        case 0x0E: {
            switch (NN) {

                case 0x9E: {
                    if (getkey(V[X])) {
                        PC += 2;
                    }
                    break;
                }

                case 0xA1: {
                    if (!getkey(V[X])) {
                        PC += 2;
                    }
                    break;
                }

            }
            break;
        }

        case 0x0F: {

            switch (NN) {

                case 0x07: {
                    V[X] = DelayTimer;
                    break;
                }

                case 0x0A: {
                    // TODO
                    break;
                }

                case 0x15: {
                    DelayTimer = V[X];
                    break;
                }

                case 0x18: {
                    SoundTimer = V[X];
                    break;
                }

                case 0x1E: {
                    I += V[X];
                    break;
                }

                case 0x29: {
                    I = 0x0 + V[X];
                    break;
                }

                case 0x33: {
                    Memory[I] = V[X] / 100;
                    Memory[I+1] = (V[X] % 100) / 10;
                    Memory[I+2] = V[X] % 10;
                    break;
                }

                case 0x55: {
                    for (byte_t i = 0; i <= X; i++) {
                        Memory[I + i] = V[i];
                    }
                    break;
                }

                case 0x65: {
                    for (byte_t i = 0; i <= X; i++) {
                        V[i] = Memory[I + i];
                    }
                    break;
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
                printf("\x1b[47m  "); // ANSI WHITE BG
            } else {
                printf("\x1b[40m  "); // ANSI BLACK BG
            }
            printf("\x1b[0m"); // ANSI RESET
        }
        putchar('\n');
    }
}

int main(int argc, char* argv[]) {

    if (argc != 2) return ERR(1, "Expected an argument");

    char* path = argv[1];

    PC = 0x200;
    I = 0x0;

    FILE* font = fopen("../resource/font.ch8", "rb");
    if (font == NULL) return ERR(1, "Could not open font file");
    ch8load(font, 80, I);
    fclose(font);

    FILE* rom = fopen(path, "rb");
    if (rom == NULL) return ERR(1, "Could not open ROM file");
    ch8load(rom, 1024, PC);
    fclose(rom);

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

            if (Draw) {
                Draw = 0;
                ch8display();
            } 

            if (DelayTimer > 0) {
                DelayTimer--;
            }

            if (SoundTimer > 0) {
                SoundTimer--;
            }

            t -= delay;
        }

    }

    return 0;
}