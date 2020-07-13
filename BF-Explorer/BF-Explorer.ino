// Command Line Interface (Serial) to explore BF files running the SD card.
// 
// Enter commands on the serial monitor to control the application.
//
// Dependencies
// SDFat at https://github.com/greiman?tab=repositories
// MD_cmdProcessor at https://github.com/MajicDesigns/MD_cmdProcessor
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

#include <SdFat.h>
#include <MD_cmdProcessor.h>

// Constants and Macros
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

const uint8_t SD_SELECT = 10;             // SD chip select pin for SPI comms

const char MEM_FILENAME[] = "MEMORY.DAT"; // memory file name
const int32_t MEMORY_SIZE = 30000;       // size of the BF memory in bytes

// Global Data
SdFat SD;

SdFile fileProg;    // program file descriptor
SdFile fileMem;     // memory file descriptor

int32_t addrIP, addrMP;   // current program and memory address
bool listRunning = false; // flag for listing the running program

enum { IDLE, RUN, STEP } runMode = IDLE;
uint16_t steps;   // keeps the number of streps to run

bool initializeMemory(void)
// initialise an open memory file with zeroes
{
  bool b = true;

  fileMem.rewind();   // back to start
  for (uint32_t i = 0; i < MEMORY_SIZE; i++)
    fileMem.write((uint8_t)0);
  fileMem.flush();
  fileMem.rewind();   // move back to the start

  // reset the current memory address
  addrMP = 0;

  return(b);
}

bool createMemory(void)
// create a new memory file
{
  bool b = true;

  if (fileMem.open(MEM_FILENAME, O_RDWR | O_CREAT))
  {
    b = initializeMemory();
  }
  else
  {
    Serial.print(F("\n!Memory create failed"));
    b = false;
  }

  return(b);
}

void handlerHelp(char* param); // function prototype only

void handlerCD(char* param)
// set the current folder for BF files
{
  Serial.print(F("folder '"));
  Serial.print(param);
  Serial.print(F("'"));
  if (!SD.chdir(param, true)) // set folder
    Serial.print(F("\n!Folder not set"));
  setIdle();
}

void handlerLS(char* param)
// list the files in the current folder
{
  SdFile file;    // iterated file

  Serial.print(F("list"));
  SD.vwd()->rewind();
  while (file.openNext(SD.vwd(), O_READ))
  {
    char buf[20];

    file.getName(buf, ARRAY_SIZE(buf));
    Serial.print(F("\n"));
    Serial.print(buf);
    if (!file.isDir())
    {
      Serial.print(F("\t"));
      Serial.print(file.fileSize());
    }
    file.close();
  }
  Serial.print(F("\n"));
  setIdle();
}

void handlerS(char* param)
// step the program ny n steps
{
  Serial.print(F("step "));
  Serial.print(param);
  runMode = STEP;
  if (*param == '\0')
    steps = 1;
  else
    steps = strtoul(param, nullptr, 0);
}

void handlerL(char* param)
// load the program specified
{
  Serial.print(F("load '"));
  Serial.print(param);
  Serial.print(F("'"));
  if (runMode == IDLE)
  {
    // if a file is already open, close it
    if (fileProg.isOpen())
      fileProg.close();

    // open the program file
    if (!fileProg.open(param, O_RDONLY))
      Serial.print(F("\n!File not found"));
  }
  setIdle();
}

void handlerR(char* param)
// run the currently loaded program
{
  Serial.print(F("run"));
  if (runMode == IDLE)
  {
    // file needs to be open
    if (!fileProg.isOpen())
    {
      Serial.print(F("!No program loaded"));
      return;
    }

    // if mem initialize successful, set up for run
    if (initializeMemory())
    {
      fileProg.rewind();
      addrIP = 0;   // start at the first address
      runMode = RUN;
      Serial.print(F("\n"));    // clean line for any output from the program
    }
    else  // can't init memory
      setIdle();
  }
}

void handlerP(char* param)
// toggle pause 
{
  if (runMode == RUN)
    setIdle();
  else
    runMode = RUN;
}

void handlerX(char* param)
// toggle list running program 
{
  Serial.print(F("listing "));
  listRunning = !listRunning;
  Serial.print(listRunning ? F("on") : F("off"));
  if (runMode == IDLE)
    setIdle();  // only for the prompt ...
}

const MD_cmdProcessor::cmdItem_t PROGMEM cmdTable[] =
{
  { "h",  handlerHelp, "",    "help", 0 },
  { "?",  handlerHelp, "",    "help", 0 },
  { "cd", handlerCD,   "fldr", "set current folder to fldr", 1 },
  { "ls", handlerLS,    "",    "list files in current folder", 1 },
  { "l",  handlerL,    "file", "load the named file", 2 },
  { "r",  handlerR,    "",     "run the current file from start conditions", 2 },
  { "s",  handlerS,    "n",    "step running program by n steps (default 1)", 2 },
  { "p",  handlerP,    "",     "toggle pause program", 3 },
  { "x",  handlerX,    "",     "toggle executing listing", 3 },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); Serial.print(F("\n")); }

uint8_t getChar(void)
// blocking read one character from the serial input
{
  uint8_t n;
  
  while (!Serial.available()) {}
  n = Serial.read();
  
  return(n);
}

void setIdle(void)
{
  runMode = IDLE;
  Serial.print(F("\n. "));
}

void setup(void)
{
  Serial.begin(9600);
  Serial.print("\n[BF Explorer]");
  Serial.print(F("\nSet serial monitor line ending to LF.\n"));

  // Initialize SD
  if (!SD.begin(SD_SELECT, SPI_FULL_SPEED))
  {
    Serial.print(F("\n!SD init fail!"));
    Serial.flush();
    while (true);
  }

  // initialize command processor
  CP.begin();

  // show the available commands
  handlerHelp(nullptr);

  createMemory();
  setIdle();
}

void loop(void)
{
  CP.run();   // command line

  if (runMode != IDLE)
  {
    uint8_t mem = fileMem.peek();   // we want to keep the file pointer the same
    int16_t opcode = fileProg.peek();

    if (opcode == -1) setIdle();  // end of file

    if (runMode == RUN || runMode == STEP)
    {
      // if showing running, then print out the data
      if (listRunning)
      {
        Serial.print(F("\nP(")); Serial.print(addrIP);
        Serial.print(F(") ")); Serial.print((char)opcode);
        Serial.print(F(" M(")); Serial.print(addrMP); Serial.print(F("):0x")); 
        Serial.print(mem, HEX); Serial.print(F(" '")); Serial.print((char)mem); Serial.print(F("'"));
      }

      switch (opcode)
      {
      case '>': ++addrMP; break;
      case '<': --addrMP; break;
      case '+': ++mem; fileMem.write(mem); break;
      case '-': --mem; fileMem.write(mem); break;
      case '.': Serial.print((char)mem); break;
      case ',': mem = getChar(); fileMem.write(mem);

      case '[':
        if (!mem)
        {
          uint16_t count = 1;
          while (count)
          {
            ++addrIP;
            if (addrIP >= MEMORY_SIZE)  // end of file
            {
              Serial.print(F("!Memory address exceeded"));
              setIdle();
              break;
            }
            fileProg.seekSet(addrIP);
            opcode = fileProg.peek();
            if (opcode == '[') ++count;
            if (opcode == ']') --count;
          }
        }
        break;

      case ']':
        if (mem)
        {
          uint16_t count = 1;
          while (count)
          {
            --addrIP;
            if (addrIP < 0)  // beginning of file
            {
              Serial.print(F("!Memory address before start"));
              setIdle();
              break;
            }
            fileProg.seekSet(addrIP);
            opcode = fileProg.peek();
            if (opcode == ']') ++count;
            if (opcode == '[') --count;
          }
        }
        break;
      }
      ++addrIP;

      // adjust the position in the files to the current offset
      fileMem.seekSet(addrMP);
      fileProg.seekSet(addrIP);

      // show new data in the memory address
      if (listRunning)
      {
        Serial.print(F("-> 0x")); 
        Serial.print(mem, HEX); Serial.print(F(" '")); Serial.print((char)mem); Serial.print(F("'"));
      }
    }

    // now deal with the running state changes if needed
    if (runMode == STEP)
    {
      steps--;
      if (steps == 0)
        setIdle();
    }
  }
}

