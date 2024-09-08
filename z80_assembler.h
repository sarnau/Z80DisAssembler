#define MAXLINELENGTH   256
#define MAXSYMBOLNAME   32
#define DEBUG           0

typedef enum {
    ILLEGAL,
    NUM,            // eine normale Zahl
    OPCODE,         // ein Opcode (0…255 = ASCII-Code, >=256 = Opcodes [s.u.])
    SYMBOL,         // ein Symbol
    STRING          // ein String
} Type;

// Ein kodierter Opcode
typedef struct {
    Type    typ;        // Typ
    long    val;        // Wert
} Command,*CommandP;

// Ausdruck für Backpatching
typedef struct RecalcList {
    struct RecalcList   *next;  // Folgeeintrag in der Liste
    uint16_t            typ;    // Wie soll der Ausdruck eingesetzt werden
                                // 0 = 1 Byte
                                // 1 = 2 Byte (low/high!)
                                // 2 = 1 Byte, PC-Relativ zur Einsetzadresse + 1
    uint16_t            adr;    // Einsetzadresse
    CommandP            c;      // Ptr auf Formel
} RecalcList,*RecalcListP;

// Symboltabellen-Eintrag
typedef struct Symbol {
    struct Symbol   *next;      // Ptr auf das nächste Symbol
    uint16_t        hash;       // Hashwert über den Symbolnamen
    uint16_t        type;       // Typ: 0 = Symbol; <>0 = Opcode, etc.
    char            name[MAXSYMBOLNAME+1];  // Symbolname
    int32_t         val;        // Wert vom Symbol
    unsigned        defined : 1;// True, wenn Symbol definiert
    unsigned        first : 1;  // True, wenn Symboleintrag bereits gültig
    RecalcListP     recalc;     // vom Symbol abhängige Ausdrücke (Backpatching!)
} Symbol,*SymbolP;

extern Command      Cmd[80];            // eine tokenisierte Zeile
extern SymbolP      SymTab[256];        // Symboltabelle (nach oberem Hashbyte geteilt)
extern uint16_t     PC;                 // aktuelle Adresse
extern uint8_t      *RAM;               // 64K-RAM vom Z80

extern RecalcListP  LastRecalc;         // zum Einsetzen des Typs bei inkompletten Formeln

void        Error(const char* s);       // Fehlermeldung ausgeben

int32_t     CalcTerm(CommandP *c);      // eine Formel ausrechnen

void        CompileLine(void);          // für die Compilierung

void        InitSymTab(void);           // für die Tokenisierung
void        TokenizeLine(char* sp);
