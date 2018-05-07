#include <elf/elf++.hh>
#include <dwarf/dwarf++.hh>
#include <fcntl.h>
#include <inttypes.h>

char* dump_line_table(const dwarf::line_table &lt, int target)
{
  for (auto &line : lt) {
    if (line.end_sequence){
      printf("line out of bounds\n");
      return NULL;
    }else if(line.line >= target){
      return (char*) line.address;
    }
  }
  return NULL;
}

extern "C" {
  char* print_lines(char* file, int line_num)
  {

    int fd = open(file, O_RDONLY);
    if (fd < 0) {
      fprintf(stderr, "%s: %s\n", file, strerror(errno));
      return NULL;
    }

    elf::elf ef(elf::create_mmap_loader(fd));
    dwarf::dwarf dw(dwarf::elf::create_loader(ef));

    for (auto cu : dw.compilation_units()) {
      printf("--- <%x>\n", (unsigned int)cu.get_section_offset());
      return dump_line_table(cu.get_line_table(), line_num);
    }

    return NULL;
  }
}
