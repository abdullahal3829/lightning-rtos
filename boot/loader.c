// Physical memory address where the CGA text mode buffer starts
#define CGA_VIDEO_MEMORY ((volatile char*)0xB8000)

// Standard CGA console metrics
#define SCREEN_COLS 80
#define SCREEN_ROWS 25

// Attribute bytes (Color format: [Background 4-bits][Foreground 4-bits])
#define COLOR_WHITE_ON_BLACK 0x07
#define COLOR_GREEN_ON_BLACK 0x02

void clear_screen() {
    volatile char* video = CGA_VIDEO_MEMORY;
    int total_chars = SCREEN_COLS * SCREEN_ROWS;
    
    for (int i = 0; i < total_chars; i++) {
        video[i * 2] = ' ';                     // Blank character ASCII
        video[i * 2 + 1] = COLOR_WHITE_ON_BLACK; // Default display attribute
    }
}

void print_string(const char* str, int row, int col, char attribute) {
    volatile char* video = CGA_VIDEO_MEMORY;
    
    // Calculate byte offset: (row * 80 columns + starting column) * 2 bytes per character
    int offset = (row * SCREEN_COLS + col) * 2;
    
    for (int i = 0; str[i] != '\0'; i++) {
        video[offset] = str[i];          // Write the ASCII text character
        video[offset + 1] = attribute;   // Write the video attribute color byte
        offset += 2;                     // Move forward by 2 bytes (1 character block)
    }
}

void lmain() {
    // Clear out residual BIOS characters from screen memory
    clear_screen();
    
    // Write "Hello World!" centered horizontally around line 12
    print_string("Hello World!", 12, 34, COLOR_GREEN_ON_BLACK);
    
    // Keep the CPU processing thread alive permanently inside 32-bit mode
    while (1) {
        __asm__ __volatile__("hlt");
    }
}