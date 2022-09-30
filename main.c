#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define int long long

int debug;
int assembly;

// 指令
enum {
    LEA,
    IMM,
    JMP,
    CALL,
    JZ,
    JNZ,
    ENT,
    ADJ,
    LEV,
    LI,
    LC,
    SI,
    SC,
    PUSH,
    OR,
    XOR,
    AND,
    EQ,
    NE,
    LT,
    GT,
    LE,
    GE,
    SHL,
    SHR,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    OPEN,
    READ,
    CLOS,
    PRTF,
    MALC,
    MSET,
    MCMP,
    EXIT
};

// 具体类型
enum {
    Num = 128,
    Fun,
    Sys,
    Glo,
    Loc,
    Id,
    Char,
    Else,
    Enum,
    If,
    Int,
    Return,
    Sizeof,
    While,
    Assign,
    Cond,
    Lor,
    Lan,
    Or,
    Xor,
    And,
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Shl,
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Inc,
    Dec,
    Brak
};

// 符号表九个区域
enum { Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize };

// 符号表第五个区域
enum { CHAR, INT, PTR };

enum { Global, Local };

int *text;      // 代码段
int *stack;     // 栈段
int *old_text;  // dump
char *data;     // 数据段
int *idmain;

char *src, *old_src;  // 源字符串

int poolsize;
int *pc;  // 下一指令寄存器指针
int *sp;  // 栈顶寄存器指针
int *bp;  // 栈基址寄存器指针

int ax;  // 通用寄存器
int cycle;

int *current_id;  // 符号表行
int *symbols;     // 符号表
int line;         // 源代码第几行
int token;        // current token

int token_val;  // 当前具体类型的值

int basetype;
int expr_type;

int index_of_bp;  // 栈基址

void Next() {
    char *last_pos;
    int hash;

    while (token = *src) {
        ++src;

        if (token == '\n') {
            if (assembly) {
                // print compile info
                printf("%d: %.*s", line, src - old_src, old_src);
                old_src = src;

                while (old_text < text) {
                    printf("%8.4s", & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                                      "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                                      "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[*++old_text * 5]);

                    if (*old_text <= ADJ)
                        printf(" %d\n", *++old_text);
                    else
                        printf("\n");
                }
            }
            ++line;
        } else if (token == '#') {
            while (*src != 0 && *src != '\n') {
                src++;
            }
        } else if ((token >= 'a' && token <= 'z') ||
                   (token >= 'A' && token <= 'Z') || (token == '_')) {
            // 标识符
            last_pos = src - 1;
            hash = token;

            while ((*src >= 'a' && *src <= 'z') ||
                   (*src >= 'A' && *src <= 'Z') ||
                   (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src;
                src++;
            }

            // 查找符号表行
            current_id = symbols;
            while (current_id[Token]) {
                if (current_id[Hash] == hash &&
                    !memcmp((char *)current_id[Name], last_pos,
                            src - last_pos)) {
                    token = current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }

            // 填充符号表一二三区域
            current_id[Name] = (int)last_pos;
            current_id[Hash] = hash;
            token = current_id[Token] = Id;
            return;
        } else if (token >= '0' && token <= '9') {
            // 立即数，分别处理十六进制、八进制、十进制
            token_val = token - '0';
            if (token_val > 0) {
                while (*src >= '0' && *src <= '9') {
                    token_val = token_val * 10 + *src++ - '0';
                }
            } else {
                if (*src == 'x' || *src == 'X') {
                    token = *++src;
                    while ((token >= '0' && token <= '9') ||
                           (token >= 'a' && token <= 'f') ||
                           (token >= 'A' && token <= 'F')) {
                        token_val = token_val * 16 + (token & 15) +
                                    (token >= 'A' ? 9 : 0);
                        token = *++src;
                    }
                } else {
                    while (*src >= '0' && *src <= '7') {
                        token_val = token_val * 8 + *src++ - '0';
                    }
                }
            }

            token = Num;
            return;
        } else if (token == '/') {
            if (*src == '/') {
                while (*src != 0 && *src != '\n') {
                    ++src;
                }
            } else {
                token = Div;
                return;
            }
        } else if (token == '"' || token == '\'') {
            last_pos = data;
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') {
                    token_val = *src++;
                    if (token_val == 'n') {
                        token_val = '\n';
                    }
                }

                if (token == '"') {
                    *data++ = token_val;
                }
            }

            src++;
            // 字符串在数据段
            if (token == '"') {
                token_val = (int)last_pos;
            } else {
                token = Num;
            }

            return;
        }
        // 运算符
        else if (token == '=') {
            if (*src == '=') {
                src++;
                token = Eq;
            } else {
                token = Assign;
            }
            return;
        } else if (token == '+') {
            if (*src == '+') {
                src++;
                token = Inc;
            } else {
                token = Add;
            }
            return;
        } else if (token == '-') {
            if (*src == '-') {
                src++;
                token = Dec;
            } else {
                token = Sub;
            }
            return;
        } else if (token == '!') {
            if (*src == '=') {
                src++;
                token = Ne;
            }
            return;
        } else if (token == '<') {
            if (*src == '=') {
                src++;
                token = Le;
            } else if (*src == '<') {
                src++;
                token = Shl;
            } else {
                token = Lt;
            }
            return;
        } else if (token == '>') {
            if (*src == '=') {
                src++;
                token = Ge;
            } else if (*src == '>') {
                src++;
                token = Shr;
            } else {
                token = Gt;
            }
            return;
        } else if (token == '|') {
            if (*src == '|') {
                src++;
                token = Lor;
            } else {
                token = Or;
            }
            return;
        } else if (token == '&') {
            if (*src == '&') {
                src++;
                token = Lan;
            } else {
                token = And;
            }
            return;
        } else if (token == '^') {
            token = Xor;
            return;
        } else if (token == '%') {
            token = Mod;
            return;
        } else if (token == '*') {
            token = Mul;
            return;
        } else if (token == '[') {
            token = Brak;
            return;
        } else if (token == '?') {
            token = Cond;
            return;
        } else if (token == '~' || token == ';' || token == '{' ||
                   token == '}' || token == '(' || token == ')' ||
                   token == ']' || token == ',' || token == ':') {
            return;
        }
    }
}

void Match(int tk) {
    if (token == tk) {
        Next();
    } else {
        printf("%d: expected token: %d\n", line, tk);
        exit(-1);
    }
}

void Expression(int level) {
    int *id;
    int tmp;
    int *addr;
    {
        if (!token) {
            printf("%d: unexpected token EOF of Expression\n", line);
            exit(-1);
        }
        if (token == Num) {
            // 立即数
            Match(Num);
            *++text = IMM;
            *++text = token_val;
            expr_type = INT;
        } else if (token == '"') {
            // 字符串字面值
            *++text = IMM;
            *++text = token_val;

            Match('"');
            while (token == '"') {
                Match('"');
            }
            data = (char *)(((int)data + sizeof(int)) & (-sizeof(int)));
            expr_type = PTR;
        } else if (token == Sizeof) {
            Match(Sizeof);
            Match('(');
            expr_type = INT;

            if (token == Int) {
                Match(Int);
            } else if (token == Char) {
                Match(Char);
                expr_type = CHAR;
            }

            while (token == Mul) {
                Match(Mul);
                expr_type = expr_type + PTR;
            }

            Match(')');

            *++text = IMM;
            *++text = (expr_type == CHAR) ? sizeof(char) : sizeof(int);

            expr_type = INT;
        } else if (token == Id) {
            // 标识符
            Match(Id);

            id = current_id;

            if (token == '(') {
                // 函数
                Match('(');

                tmp = 0;  // 参数个数
                while (token != ')') {
                    Expression(Assign);
                    *++text = PUSH;
                    tmp++;

                    if (token == ',') {
                        Match(',');
                    }
                }
                Match(')');

                if (id[Class] == Sys) {
                    // 系统函数

                    *++text = id[Value];
                } else if (id[Class] == Fun) {
                    // 普通函数
                    *++text = CALL;
                    *++text = id[Value];
                } else {
                    printf("%d: bad function call\n", line);
                    exit(-1);
                }

                // 回收传入参数
                if (tmp > 0) {
                    *++text = ADJ;
                    *++text = tmp;
                }
                expr_type = id[Type];
            } else if (id[Class] == Num) {
                *++text = IMM;
                *++text = id[Value];
                expr_type = INT;
            } else {
                // 变量
                if (id[Class] == Loc) {
                    // 局部变量
                    *++text = LEA;
                    *++text = index_of_bp - id[Value];
                } else if (id[Class] == Glo) {
                    // 全局变量
                    *++text = IMM;
                    *++text = id[Value];
                } else {
                    printf("%d: undefined variable\n", line);
                    exit(-1);
                }

                expr_type = id[Type];
                *++text = (expr_type == CHAR) ? LC : LI;
            }
        } else if (token == '(') {
            // 显式类型转换或者左括号
            Match('(');
            if (token == Int || token == Char) {
                tmp = (token == Char) ? CHAR : INT;  // 显式类型转换
                Match(token);
                while (token == Mul) {
                    Match(Mul);
                    tmp = tmp + PTR;
                }

                Match(')');
                // 递归获取后面的变量，修改类型信息
                Expression(Inc);

                expr_type = tmp;
            } else {
                // 左括号
                Expression(Assign);
                Match(')');
            }
        } else if (token == Mul) {
            // 解引用
            Match(Mul);
            // 递归获取后面变量
            Expression(Inc);

            if (expr_type >= PTR) {
                // 变量类型3*n减少一个指针
                expr_type = expr_type - PTR;
            } else {
                printf("%d: bad dereference\n", line);
                exit(-1);
            }
            // LI解引用
            *++text = (expr_type == CHAR) ? LC : LI;
        } else if (token == And) {
            // 取地址
            Match(And);
            // 递归，最后遇到变量
            Expression(Inc);
            // 变量肯定会被LI将变量地址的数据放到通用寄存器，只需要删除LI，则通用寄存器存放变量地址
            if (*text == LC || *text == LI) {
                text--;
            } else {
                printf("%d: bad address of\n", line);
                exit(-1);
            }

            expr_type = expr_type + PTR;
        } else if (token == '!') {
            // 逻辑非
            Match('!');
            Expression(Inc);
            *++text = PUSH;
            *++text = IMM;
            *++text = 0;
            *++text = EQ;

            expr_type = INT;
        } else if (token == '~') {
            // 位取反
            Match('~');
            Expression(Inc);

            *++text = PUSH;
            *++text = IMM;
            *++text = -1;
            *++text = XOR;

            expr_type = INT;
        } else if (token == Add) {
            // 正数
            Match(Add);
            Expression(Inc);

            expr_type = INT;
        } else if (token == Sub) {
            // 负数
            Match(Sub);

            if (token == Num) {
                *++text = IMM;
                *++text = -token_val;
                Match(Num);
            } else {
                *++text = IMM;
                *++text = -1;
                *++text = PUSH;
                Expression(Inc);
                *++text = MUL;
            }

            expr_type = INT;
        } else if (token == Inc || token == Dec) {
            // 前置++ --
            tmp = token;
            Match(token);
            Expression(Inc);
            // 递归后，最后肯定遇到变量，变量地址在栈顶，变量值在通用寄存器，将值放到栈顶，IMM
            // 1后ADD，SI保存到变量地址
            if (*text == LC) {
                *text = PUSH;
                *++text = LC;
            } else if (*text == LI) {
                *text = PUSH;
                *++text = LI;
            } else {
                printf("%d: bad lvalue of pre-increment\n", line);
                exit(-1);
            }
            *++text = PUSH;
            *++text = IMM;
            *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text = (tmp == Inc) ? ADD : SUB;
            *++text = (expr_type == CHAR) ? SC : SI;
        } else {
            printf("%d: bad Expression\n", line);
            exit(-1);
        }
    }

    {
        // 二元运算符，通常是之前遇到数据，然后来到二元运算符
        while (token >= level) {
            tmp = expr_type;
            if (token == Assign) {
                // 赋值，左边必定是变量，且变量地址在栈顶，变量值在通用寄存器
                Match(Assign);
                if (*text == LC || *text == LI) {
                    *text = PUSH;
                } else {
                    printf("%d: bad lvalue in assignment\n", line);
                    exit(-1);
                }
                Expression(Assign);

                expr_type = tmp;
                *++text = (expr_type == CHAR) ? SC : SI;
            } else if (token == Cond) {
                //  ? a : b;
                Match(Cond);
                *++text = JZ;
                addr = ++text;
                Expression(Assign);
                if (token == ':') {
                    Match(':');
                } else {
                    printf("%d: missing colon in conditional\n", line);
                    exit(-1);
                }
                *addr = (int)(text + 3);
                *++text = JMP;
                addr = ++text;
                Expression(Cond);
                *addr = (int)(text + 1);
            } else if (token == Lor) {
                // 逻辑或
                Match(Lor);
                *++text = JNZ;
                addr = ++text;
                Expression(Lan);
                *addr = (int)(text + 1);
                expr_type = INT;
            } else if (token == Lan) {
                Match(Lan);
                *++text = JZ;
                addr = ++text;
                Expression(Or);
                *addr = (int)(text + 1);
                expr_type = INT;
            } else if (token == Or) {
                Match(Or);
                *++text = PUSH;
                Expression(Xor);
                *++text = OR;
                expr_type = INT;
            } else if (token == Xor) {
                Match(Xor);
                *++text = PUSH;
                Expression(And);
                *++text = XOR;
                expr_type = INT;
            } else if (token == And) {
                Match(And);
                *++text = PUSH;
                Expression(Eq);
                *++text = AND;
                expr_type = INT;
            } else if (token == Eq) {
                Match(Eq);
                *++text = PUSH;
                Expression(Ne);
                *++text = EQ;
                expr_type = INT;
            } else if (token == Ne) {
                Match(Ne);
                *++text = PUSH;
                Expression(Lt);
                *++text = NE;
                expr_type = INT;
            } else if (token == Lt) {
                Match(Lt);
                *++text = PUSH;
                Expression(Shl);
                *++text = LT;
                expr_type = INT;
            } else if (token == Gt) {
                Match(Gt);
                *++text = PUSH;
                Expression(Shl);
                *++text = GT;
                expr_type = INT;
            } else if (token == Le) {
                Match(Le);
                *++text = PUSH;
                Expression(Shl);
                *++text = LE;
                expr_type = INT;
            } else if (token == Ge) {
                Match(Ge);
                *++text = PUSH;
                Expression(Shl);
                *++text = GE;
                expr_type = INT;
            } else if (token == Shl) {
                Match(Shl);
                *++text = PUSH;
                Expression(Add);
                *++text = SHL;
                expr_type = INT;
            } else if (token == Shr) {
                Match(Shr);
                *++text = PUSH;
                Expression(Add);
                *++text = SHR;
                expr_type = INT;
            } else if (token == Add) {
                // 加法
                Match(Add);
                *++text = PUSH;
                Expression(Mul);

                expr_type = tmp;
                if (expr_type > PTR) {
                    // 左边是指针，需要a+b->a+b*8
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                }
                *++text = ADD;
            } else if (token == Sub) {
                // 减法
                Match(Sub);
                *++text = PUSH;
                Expression(Mul);
                if (tmp > PTR && tmp == expr_type) {
                    // 指针-指针，a-b需要(a-b)/8
                    *++text = SUB;
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = DIV;
                    expr_type = INT;
                } else if (tmp > PTR) {
                    // 指针-整数，a-b需要a-b*8
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                    *++text = SUB;
                    expr_type = tmp;
                } else {
                    *++text = SUB;
                    expr_type = tmp;
                }
            } else if (token == Mul) {
                Match(Mul);
                *++text = PUSH;
                Expression(Inc);
                *++text = MUL;
                expr_type = tmp;
            } else if (token == Div) {
                Match(Div);
                *++text = PUSH;
                Expression(Inc);
                *++text = DIV;
                expr_type = tmp;
            } else if (token == Mod) {
                Match(Mod);
                *++text = PUSH;
                Expression(Inc);
                *++text = MOD;
                expr_type = tmp;
            } else if (token == Inc || token == Dec) {
                // 后置++，--，则前面肯定遇到变量，此时变量地址在栈顶，变量值在通用寄存器
                if (*text == LI) {
                    *text = PUSH;
                    *++text = LI;
                } else if (*text == LC) {
                    *text = PUSH;
                    *++text = LC;
                } else {
                    printf("%d: bad value in increment\n", line);
                    exit(-1);
                }

                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? ADD : SUB;
                *++text = (expr_type == CHAR) ? SC : SI;
                *++text = PUSH;
                *++text = IMM;
                *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
                *++text = (token == Inc) ? SUB : ADD;
                Match(token);
            } else if (token == Brak) {
                // a[x] = *(a + x)
                Match(Brak);
                *++text = PUSH;
                Expression(Assign);
                Match(']');

                if (tmp > PTR) {
                    *++text = PUSH;
                    *++text = IMM;
                    *++text = sizeof(int);
                    *++text = MUL;
                } else if (tmp < PTR) {
                    printf("%d: pointer type expected\n", line);
                    exit(-1);
                }
                expr_type = tmp - PTR;
                *++text = ADD;
                *++text = (expr_type == CHAR) ? LC : LI;
            } else {
                printf("%d: compiler error, token = %d\n", line, token);
                exit(-1);
            }
        }
    }
}

void Statement() {
    int *a;  // 第一个跳转点
    int *b;  // 第二个跳转点

    if (token == If) {
        // a指向符合区域的最后，b指向结束区域，a作为JZ参数，b作为JMP参数
        Match(If);
        Match('(');
        Expression(Assign);
        Match(')');

        *++text = JZ;
        b = ++text;

        Statement();
        if (token == Else) {
            Match(Else);

            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;

            Statement();
        }

        *b = (int)(text + 1);
    } else if (token == While) {
        // a指向while之前，b指向结束区域，a作为JMP参数，b作为JZ参数
        Match(While);

        a = text + 1;

        Match('(');
        Expression(Assign);
        Match(')');

        *++text = JZ;
        b = ++text;

        Statement();

        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text + 1);
    } else if (token == '{') {
        Match('{');

        while (token != '}') {
            Statement();
        }

        Match('}');
    } else if (token == Return) {
        Match(Return);

        if (token != ';') {
            Expression(Assign);
        }

        Match(';');

        *++text = LEV;
    } else if (token == ';') {
        Match(';');
    } else {
        Expression(Assign);
        Match(';');
    }
}

void EnumDeclaration() {
    int i;
    i = 0;
    while (token != '}') {
        if (token != Id) {
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        Next();
        if (token == Assign) {
            Next();
            if (token != Num) {
                printf("%d: bad enum initializer\n", line);
                exit(-1);
            }
            i = token_val;
            Next();
        }

        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;

        if (token == ',') {
            Next();
        }
    }
}

void FunctionParameter() {
    int type;
    int params;
    params = 0;
    while (token != ')') {
        type = INT;
        if (token == Int) {
            Match(Int);
        } else if (token == Char) {
            type = CHAR;
            Match(Char);
        }

        while (token == Mul) {
            Match(Mul);
            type = type + PTR;
        }

        if (token != Id) {
            printf("%d: bad parameter declaration\n", line);
            exit(-1);
        }
        if (current_id[Class] == Loc) {
            printf("%d: duplicate parameter declaration\n", line);
            exit(-1);
        }

        Match(Id);
        current_id[BClass] = current_id[Class];
        current_id[Class] = Loc;
        current_id[BType] = current_id[Type];
        current_id[Type] = type;
        current_id[BValue] = current_id[Value];
        current_id[Value] = params++;
        if (token == ',') {
            Match(',');
        }
    }
    index_of_bp = params + 1;
}

void FunctionBody() {
    int pos_local;
    int type;
    pos_local = index_of_bp;

    while (token == Int || token == Char) {
        // 函数体的局部变量定义要求全部在开头
        basetype = (token == Int) ? INT : CHAR;
        Match(token);

        while (token != ';') {
            type = basetype;
            while (token == Mul) {
                Match(Mul);
                type = type + PTR;
            }

            if (token != Id) {
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            if (current_id[Class] == Loc) {
                printf("%d: duplicate local declaration\n", line);
                exit(-1);
            }
            Match(Id);

            current_id[BClass] = current_id[Class];
            current_id[Class] = Loc;
            current_id[BType] = current_id[Type];
            current_id[Type] = type;
            current_id[BValue] = current_id[Value];
            current_id[Value] = ++pos_local;
            if (token == ',') {
                Match(',');
            }
        }
        Match(';');
    }

    // ENT指令
    *++text = ENT;
    // ENT指令参数是局部变量个数
    *++text = pos_local - index_of_bp;

    while (token != '}') {
        Statement();
    }

    *++text = LEV;
}

void FunctionDeclaration() {
    Match('(');
    FunctionParameter();
    Match(')');
    Match('{');
    FunctionBody();
    current_id = symbols;
    while (current_id[Token]) {
        if (current_id[Class] == Loc) {
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
}

void GlobalDeclaration() {
    int type;
    int i;

    basetype = INT;

    if (token == Enum) {
        // 枚举
        Match(Enum);
        if (token != '{') {
            Match(Id);
        }
        if (token == '{') {
            Match('{');
            EnumDeclaration();
            Match('}');
        }

        Match(';');
        return;
    }

    // 函数或者变量
    if (token == Int) {
        Match(Int);
    } else if (token == Char) {
        Match(Char);
        basetype = CHAR;
    }

    // parse the comma seperated variable declaration.
    while (token != ';' && token != '}') {
        type = basetype;
        // 指针
        while (token == Mul) {
            Match(Mul);
            type = type + PTR;
        }

        if (token != Id) {
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if (current_id[Class]) {
            printf("%d: duplicate global declaration\n", line);
            exit(-1);
        }
        Match(Id);
        current_id[Type] = type;

        if (token == '(') {
            // 函数
            current_id[Class] = Fun;
            current_id[Value] =
                (int)(text + 1);  // the memory address of function
            FunctionDeclaration();
        } else {
            // 变量
            current_id[Class] = Glo;
            current_id[Value] = (int)data;
            data = data + sizeof(int);
        }

        if (token == ',') {
            Match(',');
        }
    }
    Next();
}

void Program() {
    Next();
    while (token > 0) {
        GlobalDeclaration();
    }
}

int Eval() {
    int op, *tmp;
    cycle = 0;
    while (1) {
        cycle++;
        op = *pc++;

        if (debug) {
            printf("%d> %.4s", cycle,
                   & "LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,"
                   "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                   "OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT"[op * 5]);
            if (op <= ADJ)
                printf(" %d\n", *pc);
            else
                printf("\n");
        }
        // 指令实现
        if (op == IMM) {
            ax = *pc++;
        }  // 参数放入通用寄存器
        else if (op == LC) {
            ax = *(char *)ax;
        }  // 加载地址的数据到通用寄存器
        else if (op == LI) {
            ax = *(int *)ax;
        }  // 加载地址的数据到通用寄存器
        else if (op == SC) {
            ax = *(char *)*sp++ = ax;
        }  //通用寄存器的数据保存到栈顶存放地地址中
        else if (op == SI) {
            *(int *)*sp++ = ax;
        }  //通用寄存器的数据保存到栈顶存放地地址中
        else if (op == PUSH) {
            *--sp = ax;
        }  // 通用寄存器的数据放到栈顶
        else if (op == JMP) {
            pc = (int *)*pc;
        }  // 参数放到下一指令寄存器
        else if (op == JZ) {
            pc = ax ? pc + 1 : (int *)*pc;
        }  // 通用寄存器为0才跳
        else if (op == JNZ) {
            pc = ax ? (int *)*pc : pc + 1;
        } else if (op == CALL) {
            *--sp = (int)(pc + 1);
            pc = (int *)*pc;
        }  // 返回地址放在栈顶，目标函数地址放到下一指令寄存器
        else if (op == ENT) {
            *--sp = (int)bp;
            bp = sp;
            sp = sp - *pc++;
        }  // 父函数的栈帧起点放到栈顶，栈基址寄存器指针指向当前函数栈帧起点，栈顶扩展为局部变量准备
        else if (op == ADJ) {
            sp = sp + *pc++;
        }  // 栈顶收缩
        else if (op == LEV) {
            sp = bp;
            bp = (int *)*sp++;
            pc = (int *)*sp++;
        }  // restore call frame and PC
        else if (op == LEA) {
            ax = (int)(bp + *pc++);
        }  // 栈顶来到栈帧起点，相当于当前函数栈帧清除，将父函数返回地址交给栈基址寄存器，将返回地址交给下一指令寄存器

        else if (op == OR) {
            ax = *sp++ | ax;
        } else if (op == XOR) {
            ax = *sp++ ^ ax;
        } else if (op == AND) {
            ax = *sp++ & ax;
        } else if (op == EQ) {
            ax = *sp++ == ax;
        } else if (op == NE) {
            ax = *sp++ != ax;
        } else if (op == LT) {
            ax = *sp++ < ax;
        } else if (op == LE) {
            ax = *sp++ <= ax;
        } else if (op == GT) {
            ax = *sp++ > ax;
        } else if (op == GE) {
            ax = *sp++ >= ax;
        } else if (op == SHL) {
            ax = *sp++ << ax;
        } else if (op == SHR) {
            ax = *sp++ >> ax;
        } else if (op == ADD) {
            ax = *sp++ + ax;
        } else if (op == SUB) {
            ax = *sp++ - ax;
        } else if (op == MUL) {
            ax = *sp++ * ax;
        } else if (op == DIV) {
            ax = *sp++ / ax;
        } else if (op == MOD) {
            ax = *sp++ % ax;
        }

        else if (op == EXIT) {
            printf("exit(%d)", *sp);
            return *sp;
        } else if (op == OPEN) {
            ax = open((char *)sp[1], sp[0]);
        } else if (op == CLOS) {
            ax = close(*sp);
        } else if (op == READ) {
            ax = read(sp[2], (char *)sp[1], *sp);
        } else if (op == PRTF) {
            tmp = sp + pc[1];
            ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5],
                        tmp[-6]);
        } else if (op == MALC) {
            ax = (int)malloc(*sp);
        } else if (op == MSET) {
            ax = (int)memset((char *)sp[2], sp[1], *sp);
        } else if (op == MCMP) {
            ax = memcmp((char *)sp[2], (char *)sp[1], *sp);
        } else {
            printf("unknown instruction:%d\n", op);
            return -1;
        }
    }
}

#undef int

int main(int argc, char **argv) {
#define int long long  // to work with 64bit address

    int i, fd;
    int *tmp;

    argc--;
    argv++;

    // 加载源文件
    if (argc > 0 && **argv == '-' && (*argv)[1] == 's') {
        assembly = 1;
        --argc;
        ++argv;
    }
    if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') {
        debug = 1;
        --argc;
        ++argv;
    }
    if (argc < 1) {
        printf("usage: ./main [-s 打印每一行源代码对应汇编指令] file ...\n");
        return -1;
    }

    if ((fd = open(*argv, 0)) < 0) {
        printf("could not open(%s)\n", *argv);
        return -1;
    }

    poolsize = 256 * 1024;
    line = 1;

    // 初始化内存
    if (!(text = malloc(poolsize))) {
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    if (!(data = malloc(poolsize))) {
        printf("could not malloc(%d) for data area\n", poolsize);
        return -1;
    }
    if (!(stack = malloc(poolsize))) {
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    if (!(symbols = malloc(poolsize))) {
        printf("could not malloc(%d) for symbol table\n", poolsize);
        return -1;
    }

    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    memset(symbols, 0, poolsize);

    old_text = text;

    src =
        "char else enum if int return sizeof while "
        "open read close printf malloc memset memcmp exit void main";

    // 初始化符号表
    i = Char;
    while (i <= While) {
        Next();
        current_id[Token] = i++;
    }
    i = OPEN;
    while (i <= EXIT) {
        Next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }

    Next();
    current_id[Token] = Char;
    Next();
    idmain = current_id;

    if (!(src = old_src = malloc(poolsize))) {
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    if ((i = read(fd, src, poolsize - 1)) <= 0) {
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;
    close(fd);

    // 语法解析
    Program();

    if (!(pc = (int *)idmain[Value])) {
        printf("main() not defined\n");
        return -1;
    }

    // dump_text();
    if (assembly) {
        // only for compile
        return 0;
    }

    // main函数入口
    sp = (int *)((int)stack + poolsize);
    *--sp = EXIT;
    *--sp = PUSH;
    tmp = sp;
    *--sp = argc;
    *--sp = (int)argv;
    *--sp = (int)tmp;

    return Eval();
}
