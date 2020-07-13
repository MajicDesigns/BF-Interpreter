// Core code to interpret BF programs.
// 
// Programs are stored in PROGMEM
//
// https://en.wikipedia.org/wiki/Brainfuck
//
// Brainfuck is an extremely minimalist programming language created in 1993.
// The language consists of eight simple single letter commands (><+-.,[]) and 
// an instruction pointer.
// It is not intended for practical use, but to challenge and amuse programmers.
//
// The eight language commands each consist of a single character:
//  >  increment the data pointer (to point to the next cell to the right).
//  <  decrement the data pointer (to point to the next cell to the left).
//  +  increment (increase by one) the byte at the data pointer.
//  -  decrement (decrease by one) the byte at the data pointer.
//  .  output the byte at the data pointer.
//  ,  accept one byte of input, storing its value in the byte at the data pointer.
//  [  if the byte at the data pointer is zero, then instead of moving the instruction 
//     pointer forward to the next command, jump it forward to the command after the 
//     matching ] command.
//  ]	 if the byte at the data pointer is nonzero, then instead of moving the instruction 
//     pointer forward to the next command, jump it back to the command after the 
//     matching [ command.
//
// BF ignores all characters except the eight commands so no special syntax for 
// comments is needed as long as the comments do not contain the command characters.
//
// BF programs at https://github.com/pablojorge/brainfuck
//



const char PROGMEM program[] = // hello world
"++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.";

/*
const char PROGMEM program[] = // fibonacci
">++++++++++>+>+[[+++++[>++++++++<-]>.<++++++[>--------<-]+<<<]>.>>[[-]<[>+<-]>>[<<+>+>-]"
"<[>+<-[>+<-[>+<-[>+<-[>+<-[>+<-[>+<-[>+<-[>+<-[>[-]>+>+<<<-[>+<-]]]]]]]]]]]+>>>]<<<]";
*/
/*
const char PROGMEM program[] = // sierpinski
">++++[<++++++++>-]>++++++++[>++++<-]>>++>>>+>>>+<<<<<<<<<<[-[->+<]>[-<+>>>.<<]>>>"
"[[->++++++++[>++++<-]>.<<[->+<]+>[->++++++++++<<+>]>.[-]>]]+<<<[-[->+<]+>[-<+>>>-[->+<]"
"++>[-<->]<<<]<<<<]++++++++++.+++.[-]<]+++++";
*/

const uint16_t MEM_SIZE = 400;

uint8_t getChar(void)
// blocking read one character from the serial input
{
  uint8_t n;

  while (!Serial.available()) {}
  n = Serial.read();

  return(n);
}

void setup(void)
{
  Serial.begin(9600);
  Serial.print(F("\n[BF run]\n\n"));
}

void loop(void)
{
  static uint16_t ip = 0;    // instruction offset
  static uint8_t memory[MEM_SIZE] = { 0 };
  static uint8_t *mp = memory;    // memory pointer

  char opcode = pgm_read_byte(program + ip);

  switch (opcode) 
  {
  case '>': ++mp; break;
  case '<': --mp; break;
  case '+': ++(*mp); break;
  case '-': --(*mp); break;
  case '.': Serial.print((char)(*mp)); break;
  case ',': *mp = getChar(); break;
  case '[':
    if (!*mp) 
    {
      uint16_t count = 1;

      while (count) 
      {
        ++ip;
        if (pgm_read_byte(program + ip) == '[') ++count;
        if (pgm_read_byte(program + ip) == ']') --count;
      }
    }
    break;
  case ']':
    if (*mp) 
    {
      uint16_t count = 1;

      while (count) 
      {
        --ip;
        if (pgm_read_byte(program + ip) == ']') ++count;
        if (pgm_read_byte(program + ip) == '[') --count;
      }
    }
    break;
  }

  if (opcode != '\0') // end of program string
    ++ip; 
}
