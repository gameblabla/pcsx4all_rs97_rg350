#ifndef DISASM_H
#define DISASM_H

typedef struct
{
  void *address;
  char *name;
} disasm_label;

void disasm_mips_instruction(uint32_t opcode, char *buffer, uint32_t pc,
 disasm_label *labels, uint32_t num_labels);

extern const char *reg_names[];

#endif
