#include <elf/elf++.hh>
#include <dwarf/dwarf++.hh>
#include <fcntl.h>
#include <inttypes.h>

// Function to return the address of a given line from the table of lines and addresses
void* dump_line_table(const dwarf::line_table &lt, int target)
{
  for (auto &line : lt) {
    if (line.end_sequence){
      printf("line out of bounds\n");
      return NULL;
    }else if(line.line >= target){
      return (void*) line.address;
    }
  }
  return NULL;
}

extern "C" {
  // Function that stores the lines and addresses of a program into a table and then returns the address of a given line using the dump_line_table function
  void* print_lines(char* file, int line_num)
  {

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "%s: %s\n", file, strerror(errno));
      return NULL;
    }

    elf::elf ef(elf::create_mmap_loader(fd));
    dwarf::dwarf dw(dwarf::elf::create_loader(ef));

    for (auto cu : dw.compilation_units()) {
      return dump_line_table(cu.get_line_table(), line_num);
    }

    return NULL;
  }
}

int dump_line_address(const dwarf::line_table &lt, unsigned target)
{
  int realline = 0;
  for (auto &line : lt) {
    if (line.end_sequence){
      printf("line out of bounds\n");
      return -1;
    }else if(line.address > target){
      return realline;
    }
    realline = line.line;
  }
  return -1;
}

extern "C" {
  int get_line(char* file,  unsigned address)
  {

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "%s: %s\n", file, strerror(errno));
      return -1;
    }

    elf::elf ef(elf::create_mmap_loader(fd));
    dwarf::dwarf dw(dwarf::elf::create_loader(ef));

    for (auto cu : dw.compilation_units()) {
      return dump_line_address(cu.get_line_table(), address);
    }

    return -1;
  }
}
