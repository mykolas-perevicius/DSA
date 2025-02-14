/*******************************************************
 * blockade.c (6×6, ANSI C90, Using Your ASCII_TEMPLATE)
 *
 * A minimal Quoridor-like game on a 6×6 board, columns a..f,
 * row labels 6..1. Uses ONLY your static const char *ASCII_TEMPLATE
 * (no references to a 7×7 'g' column).
 *
 * Compile:
 *   gcc -ansi -Wall -Wextra -Wpedantic -Werror blockade.c -o blockade
 * Usage:
 *   ./blockade input.txt
 *******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -----------------------------
   1) Data Representation
   ----------------------------- */

#define ROWS 6
#define COLS 6
#define MAX_FENCES 8

typedef struct {
    int row; /* 0..5 (0=bottom, 5=top) */
    int col; /* 0..5 (a..f) */
} PlayerPosition;

typedef struct {
    int rowCenter;
    int colCenter;
    char orientation; /* 'H' or 'V' */
    int isActive;     /* 0 or 1 */
} Fence;

/* Global data */
static char board[ROWS][COLS];
static PlayerPosition tortoisePos;
static PlayerPosition harePos;
static Fence tortoiseFences[MAX_FENCES];
static Fence hareFences[MAX_FENCES];
static int tortoiseFencesUsed = 0;
static int hareFencesUsed = 0;

/* Game state */
static int currentPlayer = 0;  /* 0=Tortoise, 1=Hare */
static int gameOver = 0;
static int winner = 0;         /* 0=none, 1=Tortoise, 2=Hare */

/* Forward declarations */
static void initializeGame(void);
static void parseCommands(FILE *fp);
static void processMove(const char *direction);
static void processFence(const char *line);
static int isBlocked(int r1, int c1, int r2, int c2);
static void checkWinCondition(void);
static void announceWinner(void);
static void fancyPrintBoard(void);

/* -----------------------------
   2) Initialization
   ----------------------------- */
static void initializeGame(void)
{
    int r, c;
    /* Fill board with '.' */
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            board[r][c] = '.';
        }
    }

    /* Example start: T at row=0 (bottom), col=2. H at row=5 (top), col=2. */
    tortoisePos.row = 0; 
    tortoisePos.col = 2;
    board[0][2] = 'T';

    harePos.row = 5;
    harePos.col = 2;
    board[5][2] = 'H';

    /* Clear fences. */
    memset(tortoiseFences, 0, sizeof(tortoiseFences));
    memset(hareFences, 0, sizeof(hareFences));
    tortoiseFencesUsed = 0;
    hareFencesUsed = 0;

    /* Tortoise goes first. */
    currentPlayer = 0;
    gameOver = 0;
    winner = 0;
}

/* -----------------------------
   3) Input Parsing
   ----------------------------- */
static void parseCommands(FILE *fp)
{
    char line[64];
    size_t len;
    char *p;

    while (!gameOver && fgets(line, sizeof(line), fp)) {
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        /* Uppercase the entire line. */
        p = line;
        while (*p) {
            *p = (char)toupper((unsigned char)*p);
            p++;
        }

        /* Distinguish moves vs. fences. */
        if (line[0] == 'N' || line[0] == 'S' ||
            line[0] == 'E' || line[0] == 'W') {
            processMove(line);
        }
        else if (line[0] == 'H' || line[0] == 'V') {
            processFence(line);
        }
        /* else ignore */
    }
}

/* -----------------------------
   Fence Checking
   ----------------------------- */
static int fenceBlocks(int r1, int c1, int r2, int c2, const Fence *f)
{
    /* For demonstration, no real fence-blocking logic yet. 
       If you want to implement a real check, do it here. */
    (void)r1; (void)c1; (void)r2; (void)c2; (void)f;
    return 0;
}

static int isBlocked(int r1, int c1, int r2, int c2)
{
    /* Edge check */
    if (r2 < 0 || r2 >= ROWS || c2 < 0 || c2 >= COLS) {
        return 1;
    }
    /* Simplified: no fence logic actually blocking. 
       If you want real logic, do:
         if (fenceBlocks(...)) return 1;
       etc.
    */
    return 0;
}

/* Convert direction => (dRow, dCol) */
static void directionToDelta(const char *dir, int *dRow, int *dCol)
{
    *dRow = 0; 
    *dCol = 0;

    if (strcmp(dir,"N")==0) { *dRow=1; }  /* Because row=0 is bottom, row=5 is top => 'N' means +1 in row. */
    else if (strcmp(dir,"S")==0) { *dRow=-1; }
    else if (strcmp(dir,"E")==0) { *dCol=1; }
    else if (strcmp(dir,"W")==0) { *dCol=-1; }
}

/* Check if T at row=5 => T wins, H at row=0 => H wins. 
   Or define your own condition. */
static void checkWinCondition(void)
{
    /* For example, T wins if row=5 (top), H wins if row=0 (bottom). */
    if (tortoisePos.row == 5) {
        gameOver = 1;
        winner = 1; /* Tortoise */
    }
    if (harePos.row == 0) {
        gameOver = 1;
        winner = 2; /* Hare */
    }
}

/* -----------------------------
   4) Movement Logic
   ----------------------------- */
static void processMove(const char *dir)
{
    int dRow, dCol;
    int newR, newC;
    PlayerPosition *me;
    PlayerPosition *opp;

    if (gameOver) {
        return;
    }

    directionToDelta(dir, &dRow, &dCol);
    if (!dRow && !dCol) {
        return; /* invalid direction */
    }

    if (currentPlayer == 0) {
        me = &tortoisePos;
        opp= &harePos;
    } else {
        me = &harePos;
        opp= &tortoisePos;
    }

    newR = me->row + dRow;
    newC = me->col + dCol;

    if (!isBlocked(me->row, me->col, newR, newC)) {
        /* If occupant is opponent => skip or do jump logic. 
           We'll do a simple step only for now. */
        if (board[newR][newC]=='.') {
            board[me->row][me->col]='.';
            me->row=newR; me->col=newC;
            board[newR][newC] = (currentPlayer==0)?'T':'H';

            currentPlayer=1-currentPlayer;
            printf("DEBUG: A move (%s) was made.\n", dir);
            fancyPrintBoard();
            printf("\n");

            checkWinCondition();
        }
    }
}

/* -----------------------------
   5) Fence Placement
   ----------------------------- */

/* parseSquare: "d2" => row=1, col=3 in 0-based. But we only have a..f => col=0..5, row=1..6 => 0..5 internally. */
static int parseSquare(const char *sq, int *row, int *col)
{
    char f, rch;
    if (strlen(sq)<2) return 0;

    f   = (char)tolower((unsigned char)sq[0]); /* 'a'..'f' */
    rch = sq[1]; /* '1'..'6' */

    if (f < 'a' || f > 'f') return 0;
    if (rch<'1' || rch>'6') return 0;

    *col = f - 'a';      /* 'a'=>0..'f'=>5 */
    /* row= '1'=>0..'6'=>5. But user might see '1' as bottom. 
       If you want '1' => row=0, '6'=> row=5. So do: */
    *row = (rch - '1');  /* '1'=>0, '6'=>5 */
    return 1;
}

static void processFence(const char *line)
{
    char orientation;
    char coord[8];
    Fence *fences;
    int *fUsed;
    int r, c;
    int i;

    if (gameOver) return;

    orientation=0;
    memset(coord,0,sizeof(coord));

    if (sscanf(line, "%c %7s", &orientation, coord) != 2) {
        return; /* parse fail */
    }
    orientation=(char)toupper((unsigned char)orientation);
    if (orientation!='H' && orientation!='V') {
        return; /* invalid orientation */
    }

    if (currentPlayer==0) {
        fences = tortoiseFences;
        fUsed  = &tortoiseFencesUsed;
    } else {
        fences = hareFences;
        fUsed  = &hareFencesUsed;
    }

    if (*fUsed >= MAX_FENCES) {
        return; /* no more fences */
    }

    if (!parseSquare(coord,&r,&c)) {
        return; /* invalid coordinate */
    }
    /* Must be in 0..4 for simplified logic, so it doesn't overflow. */
    if (r<0 || r>=ROWS-1 || c<0 || c>=COLS-1) {
        return;
    }

    /* Check overlap with existing fences. */
    for (i=0;i<MAX_FENCES;i++){
        if (fences[i].isActive &&
            fences[i].rowCenter==r && 
            fences[i].colCenter==c && 
            fences[i].orientation==orientation){
            return; /* exact fence already placed */
        }
    }

    /* Place new fence. */
    fences[*fUsed].rowCenter=r;
    fences[*fUsed].colCenter=c;
    fences[*fUsed].orientation=orientation;
    fences[*fUsed].isActive=1;
    (*fUsed)++;

    printf("DEBUG: Player %s placed fence '%c %s'.\n",
           (currentPlayer==0)?"Tortoise":"Hare",
           orientation, coord);
    fancyPrintBoard();
    printf("\n");

    currentPlayer=1-currentPlayer;
}

/* -----------------------------
   7) Fancy ASCII Board (6 columns)
   ----------------------------- */

static void fancyPrintBoard(void)
{
    /* Your exact ASCII template for 6 columns: */
    static const char *ASCII_TEMPLATE[] = {
        "                    [N]",
        "",
        "            a   b   c   d   e   f",
        "            |   |   |   |   |   |",
        "        +---------------------------+",
        "        |                           |",  /* Row 5 (top) */
        "     6--|   +   +   +   +   +   +   |--6        Player (H)",
        "        |                           |           Fences - 8",
        "     5--|   +   +   +   +   +   +   |--5        ==========",
        "        |                           |            | | | |",
        "     4--|   +   +   +   +   +   +   |--4         | | | |",
        "[W]     |                           |     [E]",
        "     3--|   +   +   +   +   +   +   |--3        Player (T)",
        "        |                           |           Fences - 8",
        "     2--|   +   +   +   +   +   +   |--2        ==========",
        "        |                           |            | | | |",
        "     1--|   +   +   +   +   +   +   |--1         | | | |",
        "        |                           |",  /* Row 0 (bottom) */
        "        +---------------------------+",
        "            |   |   |   |   |   |",
        "            a   b   c   d   e   f",
        "",
        "                     [S]",
    };

    #define TEMPLATE_LINES (sizeof(ASCII_TEMPLATE)/sizeof(ASCII_TEMPLATE[0]))

    char buffer[TEMPLATE_LINES][128];
    int i;

    /* Copy the template into a buffer we can modify. */
    for (i=0; i<(int)TEMPLATE_LINES; i++){
        strncpy(buffer[i], ASCII_TEMPLATE[i], 127);
        buffer[i][127] = '\0';
    }

    /*
     * We'll define row->line mapping:
     *   row=5 => line=5  (the one labeled "     6--| ... |--6")
     *   row=4 => line=7
     *   row=3 => line=9
     *   row=2 => line=11
     *   row=1 => line=13
     *   row=0 => line=15
     *
     * This is lineIndex = 5 + 2*(5 - row).
     *
     * For columns (a..f => col=0..5), the plus signs are at offset:
     *   col=0 => 9, col=1 => 13, col=2 =>17, col=3 =>21, col=4 =>25, col=5 =>29.
     * So offset = 9 + 4*col.
     */

    /* Place Tortoise */
    {
        int r = tortoisePos.row;
        int c = tortoisePos.col;
        int lineIndex = 5 + 2*(5 - r); /* row=5 =>5, row=0=>15 */
        int offset    = 9 + 4*c;      /* col=0=>9.. col=5=>29 */

        if (lineIndex >=0 && lineIndex < (int)TEMPLATE_LINES) {
            if (offset >=0 && offset < (int)strlen(buffer[lineIndex])) {
                buffer[lineIndex][offset] = 'T';
            }
        }
    }

    /* Place Hare */
    {
        int r = harePos.row;
        int c = harePos.col;
        int lineIndex = 5 + 2*(5 - r);
        int offset    = 9 + 4*c;

        if (lineIndex>=0 && lineIndex<(int)TEMPLATE_LINES) {
            if (offset>=0 && offset<(int)strlen(buffer[lineIndex])) {
                buffer[lineIndex][offset] = 'H';
            }
        }
    }

    /* Place fences (very simple approach). If orientation='H', replace '+' with '='; if 'V', we place '|'. */
    for (i=0; i<MAX_FENCES; i++){
        /* Tortoise fence i */
        if (tortoiseFences[i].isActive) {
            int fr = tortoiseFences[i].rowCenter;
            int fc = tortoiseFences[i].colCenter;
            char ori = tortoiseFences[i].orientation;
            int lineIndex = 5 + 2*(5 - fr);
            int offset    = 9 + 4*fc;

            if (ori=='H') {
                if (lineIndex>=0 && lineIndex<(int)TEMPLATE_LINES) {
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset] = '=';
                    }
                    if ((offset+4)<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset+4] = '=';
                    }
                }
            } else { /* 'V' */
                /* We'll place '|' in this row and the next ASCII row (lineIndex+2). */
                if (lineIndex>=0 && (lineIndex+2)<(int)TEMPLATE_LINES) {
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset] = '|';
                    }
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex+2])) {
                        buffer[lineIndex+2][offset] = '|';
                    }
                }
            }
        }

        /* Hare fence i */
        if (hareFences[i].isActive) {
            int fr = hareFences[i].rowCenter;
            int fc = hareFences[i].colCenter;
            char ori = hareFences[i].orientation;
            int lineIndex = 5 + 2*(5 - fr);
            int offset    = 9 + 4*fc;

            if (ori=='H') {
                if (lineIndex>=0 && lineIndex<(int)TEMPLATE_LINES) {
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset] = '=';
                    }
                    if ((offset+4)<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset+4] = '=';
                    }
                }
            } else {
                if (lineIndex>=0 && (lineIndex+2)<(int)TEMPLATE_LINES) {
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex])) {
                        buffer[lineIndex][offset] = '|';
                    }
                    if (offset>=0 && offset<(int)strlen(buffer[lineIndex+2])) {
                        buffer[lineIndex+2][offset] = '|';
                    }
                }
            }
        }
    }

    /* Print the final lines */
    for (i=0; i<(int)TEMPLATE_LINES; i++){
        printf("%s\n", buffer[i]);
    }
}

/* -----------------------------
   6) Announce Winner
   ----------------------------- */
static void announceWinner(void)
{
    if (winner==1) {
        printf("Tortoise wins!\n");
    } else if (winner==2) {
        printf("Hare wins!\n");
    }
}

/* -----------------------------
   7) Main
   ----------------------------- */
int main(int argc, char*argv[])
{
    FILE *fp;
    if (argc<2) {
        fprintf(stderr,"Usage: %s <inputfile>\n", argv[0]);
        return 1;
    }
    fp = fopen(argv[1],"r");
    if(!fp) {
        perror("Cannot open input file");
        return 1;
    }

    initializeGame(); 
    parseCommands(fp);
    fclose(fp);

    /* Print final board */
    fancyPrintBoard();

    announceWinner();
    return 0;
}
