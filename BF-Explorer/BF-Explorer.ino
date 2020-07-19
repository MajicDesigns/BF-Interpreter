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

class cVirtualMemory
{
public:
  cVirtualMemory(uint16_t pageSize):
    _pageData(nullptr), _pageSize(pageSize)
  {}

  ~cVirtualMemory()
  {
    free(_pageData);
  }

  bool isLoaded(void) { return(_fp.isOpen()); }
  bool isInBounds(uint32_t addr) { return (addr <= _fileSize); }

  bool begin(const char* fname)
  {
    if (_fp.isOpen())
      _fp.close();

    if (_pageData == nullptr)     // allocate the memory required
      _pageData = (uint8_t*)malloc(_pageSize * sizeof(uint8_t));

    if (_pageData != nullptr)  // open file and load first page
    {
      _fp.open(fname, O_RDWR);
      _fileSize = _fp.fileSize();
      loadPage(0);
    }

    return(_pageData != nullptr && _fp.isOpen());
  }

  uint8_t get(uint32_t addr)
  {
    if (addr < _pageBaseAddr || addr >= _pageBaseAddr + _pageSize)
    {
      // save the page if it has changed
      savePage(_pageBaseAddr);

      // load the new page
      _pageBaseAddr = (addr / _pageSize) * _pageSize; 
      loadPage(_pageBaseAddr);
    }

    return(_pageData[addr - _pageBaseAddr]);
  }

  void set(uint32_t addr, uint8_t value)
  {
    if (addr < _pageBaseAddr || addr >= _pageBaseAddr + _pageSize)
    {
      // save the page if it has changed
      savePage(_pageBaseAddr);

      // load the new page
      _pageBaseAddr = (addr / _pageSize) * _pageSize;
      loadPage(_pageBaseAddr);
    }

    _pageData[addr - _pageBaseAddr] = value;
    _pageChanged = true;
  }

private:
  SdFile _fp;    // actual swap file swap file
  uint32_t _fileSize;     // size of the file
  uint8_t* _pageData;
  uint32_t _pageBaseAddr;
  uint16_t _pageSize;
  bool _pageChanged;

  void loadPage(uint32_t addr)
  {
    _fp.seekSet(addr);
    _fp.read(_pageData, _pageSize);
    _pageBaseAddr = addr;
    _pageChanged = false;
  }

  void savePage(uint32_t addr)
  {
    if (_pageChanged)
    {
      _fp.seekSet(addr);
      _fp.write(_pageData, _pageSize);
    }
  }
};

// Global Data
SdFat SD;
cVirtualMemory program(200), memory(100);

int32_t addrIP, addrMP;   // current program and memory address
bool listRunning = false; // flag for listing the running program

enum { IDLE, RUN, STEP } runMode = IDLE;
uint16_t steps;   // keeps the number of streps to run

void clearMemory(void)
// create and initialize the memory data
{
  for (uint32_t i = 0; i < MEMORY_SIZE; i++)
    memory.set(i, 0);
  Serial.print(F("\nMemory cleared."));
}

bool createMemory(void)
// create and initialise the memory file
{
 bool b = true;
 SdFile fp;

  if (fp.open(MEM_FILENAME, O_RDWR | O_CREAT))
  {
    for (uint32_t i = 0; i < MEMORY_SIZE; i++)
      fp.write((uint8_t)0);
    fp.close();
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
    if (program.begin(param))
    {
      clearMemory();
      addrMP = addrIP = 0;   // start at the first address
    }
    else
      Serial.print(F("\n!File not found"));
  }
  setIdle();
}

void handlerR(char* param)
// run the currently loaded program
{
  if (runMode == IDLE)
  {
    Serial.print(F("run"));

    // file needs to be open
    if (!program.isLoaded())
    {
      Serial.print(F("!No program loaded"));
      return;
    }

    // set up for run
    runMode = RUN;
    Serial.print(F("\n"));    // clean line for any output from the program
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

void handlerT(char* param)
// toggle trace list running program 
{
  Serial.print(F("trace "));
  listRunning = !listRunning;
  Serial.print(listRunning ? F("on") : F("off"));
  if (runMode == IDLE)
    setIdle();  // only for the prompt ...
}

char htoa(uint8_t n)
{
  char c;

  if (n < 10)
    c = n + '0';
  else if (n >= 10 && n <= 15)
    c = 'a' + n - 10;
  else
    c = '?';

  return(c);
}

void handlerD(char* param)
{
  const uint8_t DUMP_SIZE = 8;
  const uint32_t addr = strtoul(param, nullptr, 0);

  char sz[(DUMP_SIZE*3) + 2 + DUMP_SIZE + 1];
  uint8_t pos = 0;  // position in th string
  uint8_t m;        // memory cell contents

  Serial.print(F("dump "));
  Serial.print(addr);

  // clear the buffer
  memset(sz, ' ', ARRAY_SIZE(sz));
  sz[ARRAY_SIZE(sz) - 1] = '\0';

  // now make up the formatted output
  for (uint8_t i = 0; i < DUMP_SIZE; i++)
  {
    m = memory.get(addr + 1);
    sz[pos++] = htoa(m >> 4);
    sz[pos++] = htoa(m & 0xf);
    pos++;
  }

  // now display ASCII equivalents
  pos += 2;
  for (uint8_t i = 0; i < DUMP_SIZE; i++)
  {
    m = memory.get(addr + 1);
    sz[pos++] = (m <= 0xa ? '.' : m);
  }

  // now display the data
  Serial.print(F("\n"));
  Serial.print(sz);

  setIdle();
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
  { "d",  handlerD,    "n",    "dump memory address n to addr+8", 3 },
  { "p",  handlerP,    "",     "toggle pause program", 3 },
  { "t",  handlerT,    "",     "toggle execution trace listing", 3 },
};

MD_cmdProcessor CP(Serial, cmdTable, ARRAY_SIZE(cmdTable));

void handlerHelp(char* param) { CP.help(); setIdle(); }

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
  memory.begin(MEM_FILENAME);
  setIdle();
}

void loop(void)
{
  CP.run();   // command line

  if (runMode != IDLE)
  {
    uint8_t mem = memory.get(addrMP);
    int16_t opcode = program.get(addrIP);

    if (!program.isInBounds(addrIP)) // IP adress is not in valid range
      setIdle();

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
      case '+': ++mem; memory.set(addrMP, mem); break;
      case '-': --mem; memory.set(addrMP, mem); break;
      case '.': Serial.print((char)mem); break;
      case ',': mem = getChar(); memory.set(addrMP, mem);

      case '[':
        if (!mem)
        {
          uint16_t count = 1;
          while (count)
          {
            ++addrIP;
            if (!program.isInBounds(addrIP))
            {
              Serial.print(F("!Instruction pointer past end of file"));
                setIdle();
                break;
            }
            opcode = program.get(addrIP);
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
            if (!program.isInBounds(addrIP))
            {
              Serial.print(F("!Instruction pointer before start"));
              setIdle();
              break;
            }
            opcode = program.get(addrIP);
            if (opcode == ']') ++count;
            if (opcode == '[') --count;
          }
        }
        break;
      }
      ++addrIP;

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

